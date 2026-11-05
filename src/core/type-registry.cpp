/*
Control Panel for OBS Studio
Copyright (C) 2026 Voidscape Development

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "type-registry.hpp"
#include "runtime.hpp"

#include <plugin-support.h>

#include <QCoreApplication>
#include <QThread>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>

namespace cp {

/* ---- defaults for the interfaces ------------------------------------ */

obs_properties_t *ActionType::properties(obs_data_t *) const
{
	return nullptr;
}
void ActionType::defaults(obs_data_t *) const {}
control_panel_state ActionType::state(obs_data_t *) const
{
	return CONTROL_PANEL_STATE_NONE;
}
QString ActionType::defaultLabel(obs_data_t *) const
{
	return {};
}

obs_properties_t *WidgetType::properties(obs_data_t *) const
{
	return nullptr;
}
void WidgetType::defaults(obs_data_t *) const {}
bool WidgetType::update(QWidget *, obs_data_t *) const
{
	return false;
}
void WidgetType::tick(QWidget *, obs_data_t *) const {}
void WidgetType::destroy(QWidget *widget) const
{
	delete widget;
}

DataPtr settingsFor(const ActionType *type, const QJsonObject &json)
{
	DataPtr data = dataFromJson(json);
	if (type)
		type->defaults(data.get());
	return data;
}

DataPtr settingsFor(const WidgetType *type, const QJsonObject &json)
{
	DataPtr data = dataFromJson(json);
	if (type)
		type->defaults(data.get());
	return data;
}

/* ---- adapters for the C API ----------------------------------------- */

namespace {

/* Copy an API struct that may be older (smaller) than ours. Fields that the
 * caller's version doesn't have stay zeroed. */
template<typename Info> bool copyInfo(const Info *in, Info &out)
{
	if (!in || in->size == 0 || in->size > 64 * 1024)
		return false;
	memset(&out, 0, sizeof(out));
	memcpy(&out, in, std::min(in->size, sizeof(Info)));
	out.size = sizeof(Info);
	return true;
}

bool validId(const char *id)
{
	/* Built-ins own the dot-less namespace. */
	return id && *id && strchr(id, '.') != nullptr;
}

class ExternalActionType final : public ActionType {
public:
	ExternalActionType(const control_panel_action_info &info) : m_info(info), m_id(info.id)
	{
		m_external = true;
		m_info.id = m_id.c_str();
	}
	~ExternalActionType() override
	{
		if (m_info.free_type_data)
			m_info.free_type_data(m_info.type_data);
	}

	QString id() const override { return QString::fromStdString(m_id); }
	QString name() const override
	{
		const char *n = m_info.get_name(m_info.type_data);
		return n ? QString::fromUtf8(n) : id();
	}
	QString category() const override { return T("Category.Plugins"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		return m_info.get_properties ? m_info.get_properties(m_info.type_data, settings) : nullptr;
	}
	void defaults(obs_data_t *settings) const override
	{
		if (m_info.get_defaults)
			m_info.get_defaults(m_info.type_data, settings);
	}
	void execute(obs_data_t *settings) const override { m_info.execute(m_info.type_data, settings); }
	bool hasState() const override { return m_info.get_state != nullptr; }
	control_panel_state state(obs_data_t *settings) const override
	{
		return m_info.get_state ? m_info.get_state(m_info.type_data, settings) : CONTROL_PANEL_STATE_NONE;
	}
	QString defaultLabel(obs_data_t *settings) const override
	{
		if (!m_info.get_default_label)
			return {};
		char *label = m_info.get_default_label(m_info.type_data, settings);
		QString result = utf8(label);
		bfree(label);
		return result;
	}

private:
	control_panel_action_info m_info;
	std::string m_id;
};

class ExternalWidgetType final : public WidgetType {
public:
	ExternalWidgetType(const control_panel_widget_info &info) : m_info(info), m_id(info.id)
	{
		m_external = true;
		m_info.id = m_id.c_str();
	}
	~ExternalWidgetType() override
	{
		if (m_info.free_type_data)
			m_info.free_type_data(m_info.type_data);
	}

	QString id() const override { return QString::fromStdString(m_id); }
	QString name() const override
	{
		const char *n = m_info.get_name(m_info.type_data);
		return n ? QString::fromUtf8(n) : id();
	}
	obs_properties_t *properties(obs_data_t *settings) const override
	{
		return m_info.get_properties ? m_info.get_properties(m_info.type_data, settings) : nullptr;
	}
	void defaults(obs_data_t *settings) const override
	{
		if (m_info.get_defaults)
			m_info.get_defaults(m_info.type_data, settings);
	}
	QWidget *create(obs_data_t *settings, QWidget *parent) const override
	{
		return static_cast<QWidget *>(m_info.create(m_info.type_data, settings, parent));
	}
	bool update(QWidget *widget, obs_data_t *settings) const override
	{
		if (!m_info.update)
			return false;
		m_info.update(m_info.type_data, widget, settings);
		return true;
	}
	void tick(QWidget *widget, obs_data_t *settings) const override
	{
		if (m_info.tick)
			m_info.tick(m_info.type_data, widget, settings);
	}
	void destroy(QWidget *widget) const override
	{
		if (m_info.destroy)
			m_info.destroy(m_info.type_data, widget);
		else
			delete widget;
	}

private:
	control_panel_widget_info m_info;
	std::string m_id;
};

TypeRegistry *g_registry = nullptr;
std::atomic_bool g_procsEnabled{false};

bool onUiThread()
{
	return QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread();
}

bool procPreconditions(const char *proc)
{
	if (!g_procsEnabled || !g_registry)
		return false;
	if (!onUiThread()) {
		obs_log(LOG_WARNING, "%s must be called from the OBS UI thread (e.g. obs_module_post_load)", proc);
		return false;
	}
	return true;
}

void procApiVersion(void *, calldata_t *cd)
{
	calldata_set_int(cd, "version", g_procsEnabled ? CONTROL_PANEL_API_VERSION : 0);
}

bool hasRequired(const control_panel_action_info &i)
{
	return i.get_name && i.execute;
}

bool hasRequired(const control_panel_widget_info &i)
{
	return i.get_name && i.create;
}

template<typename Info, typename Adapter, typename AddFn>
void registerExternal(calldata_t *cd, const char *proc, const char *what, AddFn add)
{
	calldata_set_bool(cd, "success", false);
	if (!procPreconditions(proc))
		return;

	Info info{};
	if (!copyInfo(static_cast<const Info *>(calldata_ptr(cd, "info")), info) || !validId(info.id) ||
	    !hasRequired(info)) {
		obs_log(LOG_WARNING, "Rejected %s type registration: invalid info (id '%s')", what,
			info.id ? info.id : "(null)");
		return;
	}
	/* Check before constructing the adapter: the adapter takes ownership of
	 * type_data, and on failure ownership must stay with the caller. */
	if (g_registry->contains(QString::fromUtf8(info.id))) {
		obs_log(LOG_WARNING, "Rejected %s type '%s': id already registered", what, info.id);
		return;
	}
	add(std::make_shared<Adapter>(info));
	calldata_set_bool(cd, "success", true);
	obs_log(LOG_INFO, "Registered %s type '%s'", what, info.id);
}

void procRegisterAction(void *, calldata_t *cd)
{
	registerExternal<control_panel_action_info, ExternalActionType>(
		cd, "control_panel_register_action_type", "action",
		[](std::shared_ptr<ExternalActionType> t) { g_registry->addAction(std::move(t)); });
}

void procRegisterWidget(void *, calldata_t *cd)
{
	registerExternal<control_panel_widget_info, ExternalWidgetType>(
		cd, "control_panel_register_widget_type", "widget",
		[](std::shared_ptr<ExternalWidgetType> t) { g_registry->addWidget(std::move(t)); });
}

void procUnregister(void *, calldata_t *cd)
{
	calldata_set_bool(cd, "success", false);
	if (!procPreconditions("control_panel_unregister_type"))
		return;
	const char *id = calldata_string(cd, "id");
	if (!validId(id))
		return; /* built-ins can't be removed from outside */
	calldata_set_bool(cd, "success", g_registry->remove(QString::fromUtf8(id)));
}

void procRequestRefresh(void *, calldata_t *)
{
	if (!g_procsEnabled)
		return;
	/* Safe from any thread. */
	if (Runtime *rt = Runtime::get())
		QMetaObject::invokeMethod(rt, &Runtime::requestRefresh, Qt::QueuedConnection);
}

} // namespace

/* ---- registry ------------------------------------------------------- */

TypeRegistry *TypeRegistry::instance()
{
	return g_registry;
}

void TypeRegistry::create()
{
	if (!g_registry)
		g_registry = new TypeRegistry();
}

void TypeRegistry::destroy()
{
	delete g_registry;
	g_registry = nullptr;
}

bool TypeRegistry::contains(const QString &id) const
{
	return m_actions.contains(id) || m_widgets.contains(id);
}

bool TypeRegistry::addAction(ActionTypePtr type)
{
	if (!type || contains(type->id()))
		return false;
	m_actions.insert(type->id(), std::move(type));
	emit typesChanged();
	return true;
}

bool TypeRegistry::addWidget(WidgetTypePtr type)
{
	if (!type || contains(type->id()))
		return false;
	m_widgets.insert(type->id(), std::move(type));
	emit typesChanged();
	return true;
}

bool TypeRegistry::remove(const QString &id)
{
	/* Keep the type alive until views have dropped their instances, so a
	 * plugin's destroy callback still runs with valid type_data. */
	ActionTypePtr action = m_actions.take(id);
	WidgetTypePtr widget = m_widgets.take(id);
	if (!action && !widget)
		return false;
	emit typesChanged();
	return true;
}

QList<ActionTypePtr> TypeRegistry::actions() const
{
	QList<ActionTypePtr> list = m_actions.values();
	std::sort(list.begin(), list.end(), [](const ActionTypePtr &a, const ActionTypePtr &b) {
		const int c = QString::localeAwareCompare(a->category(), b->category());
		return c != 0 ? c < 0 : QString::localeAwareCompare(a->name(), b->name()) < 0;
	});
	return list;
}

QList<WidgetTypePtr> TypeRegistry::widgets() const
{
	QList<WidgetTypePtr> list = m_widgets.values();
	std::sort(list.begin(), list.end(), [](const WidgetTypePtr &a, const WidgetTypePtr &b) {
		return QString::localeAwareCompare(a->name(), b->name()) < 0;
	});
	return list;
}

/* ---- procs ---------------------------------------------------------- */

void registerApiProcs()
{
	static bool added = false;
	g_procsEnabled = true;
	if (added)
		return;
	added = true;

	proc_handler_t *ph = obs_get_proc_handler();
	proc_handler_add(ph, "void control_panel_get_api_version(out int version)", procApiVersion, nullptr);
	proc_handler_add(ph,
			 "void control_panel_register_action_type(in ptr info, in int api_version, out bool success)",
			 procRegisterAction, nullptr);
	proc_handler_add(ph,
			 "void control_panel_register_widget_type(in ptr info, in int api_version, out bool success)",
			 procRegisterWidget, nullptr);
	proc_handler_add(ph, "void control_panel_unregister_type(in string id, out bool success)", procUnregister,
			 nullptr);
	proc_handler_add(ph, "void control_panel_request_refresh()", procRequestRefresh, nullptr);
}

void disableApiProcs()
{
	g_procsEnabled = false;
}

} // namespace cp

/*
Control Panel for OBS Studio
Copyright (C) 2026 Voidscape Development

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include "control-panel-api.h"
#include "obs-util.hpp"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <memory>

class QWidget;

namespace cp {

/* Something a button can do. Built-ins subclass this directly; plugin types
 * registered through the C API are wrapped by an adapter. */
class ActionType {
public:
	virtual ~ActionType() = default;

	virtual QString id() const = 0;
	virtual QString name() const = 0;
	/* Grouping in the "Add action" menu. */
	virtual QString category() const = 0;

	virtual obs_properties_t *properties(obs_data_t *settings) const;
	virtual void defaults(obs_data_t *settings) const;
	virtual void execute(obs_data_t *settings) const = 0;

	/* Whether state() is meaningful (used for the auto indicator). */
	virtual bool hasState() const { return false; }
	virtual control_panel_state state(obs_data_t *settings) const;
	virtual QString defaultLabel(obs_data_t *settings) const;

	bool isExternal() const { return m_external; }

protected:
	bool m_external = false;
};

/* A cell rendered entirely by the type. */
class WidgetType {
public:
	virtual ~WidgetType() = default;

	virtual QString id() const = 0;
	virtual QString name() const = 0;

	virtual obs_properties_t *properties(obs_data_t *settings) const;
	virtual void defaults(obs_data_t *settings) const;

	virtual QWidget *create(obs_data_t *settings, QWidget *parent) const = 0;
	/* Returns false if the widget must be re-created instead. */
	virtual bool update(QWidget *widget, obs_data_t *settings) const;
	virtual void tick(QWidget *widget, obs_data_t *settings) const;
	virtual void destroy(QWidget *widget) const;

	bool isExternal() const { return m_external; }

protected:
	bool m_external = false;
};

using ActionTypePtr = std::shared_ptr<const ActionType>;
using WidgetTypePtr = std::shared_ptr<const WidgetType>;

/* Settings for a type: user values from JSON with the type's defaults. */
DataPtr settingsFor(const ActionType *type, const QJsonObject &json);
DataPtr settingsFor(const WidgetType *type, const QJsonObject &json);

class TypeRegistry : public QObject {
	Q_OBJECT

public:
	static TypeRegistry *instance(); /* nullptr outside module lifetime */
	static void create();
	static void destroy();

	bool addAction(ActionTypePtr type);
	bool addWidget(WidgetTypePtr type);
	bool remove(const QString &id);
	bool contains(const QString &id) const;

	ActionTypePtr action(const QString &id) const { return m_actions.value(id); }
	WidgetTypePtr widget(const QString &id) const { return m_widgets.value(id); }

	/* Sorted by category, then name. */
	QList<ActionTypePtr> actions() const;
	QList<WidgetTypePtr> widgets() const;

signals:
	/* Emitted after types were added or removed. Views rebuild so they never
	 * hold on to an unregistered type longer than necessary. */
	void typesChanged();

private:
	TypeRegistry() = default;

	QHash<QString, ActionTypePtr> m_actions;
	QHash<QString, WidgetTypePtr> m_widgets;
};

/* Install / remove the control_panel_* procs on the global proc handler. */
void registerApiProcs();
/* After this the procs become inert (libobs has no proc removal). */
void disableApiProcs();

void registerBuiltinActions(TypeRegistry &registry);
void registerBuiltinWidgets(TypeRegistry &registry);

} // namespace cp

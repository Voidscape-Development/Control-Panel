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

#include "properties-form.hpp"

#include <QCheckBox>
#include <QCoreApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace cp {

/* ===================================================================== */
/* Wheel guard                                                           */
/* ===================================================================== */

namespace {

class WheelGuard : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		auto *w = qobject_cast<QWidget *>(watched);
		if (event->type() != QEvent::Wheel || !w || w->hasFocus() || !w->parentWidget())
			return false;
		/* Hand the scroll to the enclosing scroll area instead. */
		QCoreApplication::sendEvent(w->parentWidget(), event);
		return true;
	}
};

WheelGuard *wheelGuard()
{
	static WheelGuard *guard = new WheelGuard(QCoreApplication::instance());
	return guard;
}

} // namespace

void guardWheel(QWidget *root)
{
	QList<QWidget *> targets;
	for (QAbstractSpinBox *w : root->findChildren<QAbstractSpinBox *>())
		targets.append(w);
	for (QComboBox *w : root->findChildren<QComboBox *>())
		targets.append(w);
	for (QSlider *w : root->findChildren<QSlider *>())
		targets.append(w);
	for (QWidget *w : targets) {
		w->setFocusPolicy(Qt::StrongFocus); /* wheel no longer grabs focus */
		w->removeEventFilter(wheelGuard());
		w->installEventFilter(wheelGuard());
	}
}

/* ===================================================================== */
/* ColorButton                                                           */
/* ===================================================================== */

ColorButton::ColorButton(QWidget *parent, bool allowUnset, bool alpha)
	: QPushButton(parent),
	  m_allowUnset(allowUnset),
	  m_alpha(alpha)
{
	setMinimumWidth(64);
	setToolTip(allowUnset ? T("Color.Tooltip") : QString());
	connect(this, &QPushButton::clicked, this, &ColorButton::pick);
}

void ColorButton::setColor(const QColor &color)
{
	if (color == m_color)
		return;
	m_color = color;
	update();
	emit colorChanged(m_color);
}

void ColorButton::pick()
{
	QColorDialog::ColorDialogOptions opts;
	if (m_alpha)
		opts |= QColorDialog::ShowAlphaChannel;
	const QColor c = QColorDialog::getColor(m_color.isValid() ? m_color : Qt::white, this, T("Color.Pick"), opts);
	if (c.isValid())
		setColor(c);
}

void ColorButton::contextMenuEvent(QContextMenuEvent *event)
{
	if (!m_allowUnset)
		return;
	QMenu menu(this);
	menu.addAction(T("Color.Reset"), this, [this]() { setColor(QColor()); });
	menu.exec(event->globalPos());
}

void ColorButton::paintEvent(QPaintEvent *event)
{
	QPushButton::paintEvent(event);
	QPainter p(this);
	const QRect r = rect().adjusted(6, 5, -6, -5);
	if (m_color.isValid()) {
		/* Checkerboard so alpha is visible. */
		const int s = 4;
		for (int y = r.top(); y < r.bottom(); y += s)
			for (int x = r.left(); x < r.right(); x += s)
				p.fillRect(QRect(x, y, s, s).intersected(r),
					   ((x - r.left()) / s + (y - r.top()) / s) % 2 ? Qt::lightGray : Qt::white);
		p.fillRect(r, m_color);
		p.setPen(palette().color(QPalette::Mid));
		p.drawRect(r.adjusted(0, 0, -1, -1));
	} else {
		p.setPen(palette().color(QPalette::ButtonText));
		p.drawText(r, Qt::AlignCenter, T("Color.Default"));
	}
}

/* ===================================================================== */
/* PropertiesForm                                                        */
/* ===================================================================== */

namespace {

QColor fromObsColor(long long v)
{
	/* OBS stores colors as 0xAABBGGRR. */
	const auto u = static_cast<uint32_t>(v);
	return QColor(u & 0xff, (u >> 8) & 0xff, (u >> 16) & 0xff, (u >> 24) & 0xff);
}

long long toObsColor(const QColor &c)
{
	return static_cast<long long>(static_cast<uint32_t>(c.red()) | (static_cast<uint32_t>(c.green()) << 8) |
				      (static_cast<uint32_t>(c.blue()) << 16) |
				      (static_cast<uint32_t>(c.alpha()) << 24));
}

} // namespace

PropertiesForm::PropertiesForm(QWidget *parent) : QWidget(parent)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
}

PropertiesForm::~PropertiesForm()
{
	obs_properties_destroy(m_props);
}

void PropertiesForm::clear()
{
	m_fn = nullptr;
	m_settings.reset();
	rebuild();
}

void PropertiesForm::setSource(PropertiesFn fn, DataPtr settings)
{
	m_fn = std::move(fn);
	m_settings = std::move(settings);
	rebuild();
}

void PropertiesForm::scheduleRebuild()
{
	/* Never rebuild inside a widget's own signal handler. */
	if (m_rebuildPending)
		return;
	m_rebuildPending = true;
	QTimer::singleShot(0, this, [this]() {
		m_rebuildPending = false;
		rebuild();
	});
}

void PropertiesForm::rebuild()
{
	/* rebuild() never runs inside one of our widgets' signal handlers (see
	 * scheduleRebuild), so the old widgets can go right away, before the
	 * properties they reference are destroyed. */
	delete m_content;
	m_content = nullptr;

	/* Re-create the properties from the current settings; applying the
	 * settings re-runs modified callbacks so dependent lists are filled. */
	obs_properties_destroy(m_props);
	m_props = nullptr;
	if (m_fn && m_settings) {
		m_props = m_fn(m_settings.get());
		if (m_props)
			obs_properties_apply_settings(m_props, m_settings.get());
	}

	m_content = new QWidget(this);
	auto *form = new QFormLayout(m_content);
	form->setContentsMargins(0, 0, 0, 0);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	m_empty = true;
	if (m_props)
		addProperties(m_props, form);
	guardWheel(m_content);
	layout()->addWidget(m_content);
}

void PropertiesForm::addProperties(obs_properties_t *props, QFormLayout *form)
{
	for (obs_property_t *p = obs_properties_first(props); p; obs_property_next(&p))
		addProperty(p, form);
}

void PropertiesForm::valueChanged(obs_property_t *prop)
{
	emit changed();
	if (obs_property_modified(prop, m_settings.get()))
		scheduleRebuild();
}

void PropertiesForm::addProperty(obs_property_t *prop, QFormLayout *form)
{
	if (!obs_property_visible(prop))
		return;

	const char *key = obs_property_name(prop);
	const QString desc = utf8(obs_property_description(prop));
	obs_data_t *s = m_settings.get();
	QWidget *field = nullptr;
	bool spansRow = false;

	switch (obs_property_get_type(prop)) {
	case OBS_PROPERTY_BOOL: {
		auto *cb = new QCheckBox(desc, m_content);
		cb->setChecked(obs_data_get_bool(s, key));
		connect(cb, &QCheckBox::toggled, this, [this, prop, key](bool v) {
			obs_data_set_bool(m_settings.get(), key, v);
			valueChanged(prop);
		});
		field = cb;
		spansRow = true;
		break;
	}
	case OBS_PROPERTY_INT: {
		auto *spin = new QSpinBox(m_content);
		spin->setRange(obs_property_int_min(prop), obs_property_int_max(prop));
		spin->setSingleStep(std::max(1, obs_property_int_step(prop)));
		spin->setSuffix(utf8(obs_property_int_suffix(prop)));
		spin->setValue(static_cast<int>(obs_data_get_int(s, key)));
		connect(spin, &QSpinBox::valueChanged, this, [this, prop, key](int v) {
			obs_data_set_int(m_settings.get(), key, v);
			valueChanged(prop);
		});
		if (obs_property_int_type(prop) == OBS_NUMBER_SLIDER) {
			auto *row = new QWidget(m_content);
			auto *h = new QHBoxLayout(row);
			h->setContentsMargins(0, 0, 0, 0);
			auto *slider = new QSlider(Qt::Horizontal, row);
			slider->setRange(spin->minimum(), spin->maximum());
			slider->setSingleStep(spin->singleStep());
			slider->setValue(spin->value());
			connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
			connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
			spin->setParent(row);
			h->addWidget(slider, 1);
			h->addWidget(spin);
			field = row;
		} else {
			field = spin;
		}
		break;
	}
	case OBS_PROPERTY_FLOAT: {
		auto *spin = new QDoubleSpinBox(m_content);
		const double step = obs_property_float_step(prop);
		spin->setRange(obs_property_float_min(prop), obs_property_float_max(prop));
		spin->setSingleStep(step > 0 ? step : 0.1);
		spin->setDecimals(step > 0 ? std::clamp(static_cast<int>(std::ceil(-std::log10(step))), 0, 6) : 2);
		spin->setSuffix(utf8(obs_property_float_suffix(prop)));
		spin->setValue(obs_data_get_double(s, key));
		connect(spin, &QDoubleSpinBox::valueChanged, this, [this, prop, key](double v) {
			obs_data_set_double(m_settings.get(), key, v);
			valueChanged(prop);
		});
		field = spin;
		break;
	}
	case OBS_PROPERTY_TEXT: {
		const obs_text_type tt = obs_property_text_type(prop);
		if (tt == OBS_TEXT_INFO) {
			auto *label = new QLabel(
				utf8(obs_data_get_string(s, key)).isEmpty() ? desc : utf8(obs_data_get_string(s, key)),
				m_content);
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			field = label;
			spansRow = true;
		} else if (tt == OBS_TEXT_MULTILINE) {
			auto *edit = new QPlainTextEdit(utf8(obs_data_get_string(s, key)), m_content);
			edit->setMaximumHeight(edit->fontMetrics().height() * 5);
			connect(edit, &QPlainTextEdit::textChanged, this, [this, prop, key, edit]() {
				obs_data_set_string(m_settings.get(), key, edit->toPlainText().toUtf8().constData());
				valueChanged(prop);
			});
			field = edit;
		} else {
			auto *edit = new QLineEdit(utf8(obs_data_get_string(s, key)), m_content);
			if (tt == OBS_TEXT_PASSWORD)
				edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
			connect(edit, &QLineEdit::textEdited, this, [this, prop, key](const QString &v) {
				obs_data_set_string(m_settings.get(), key, v.toUtf8().constData());
				valueChanged(prop);
			});
			field = edit;
		}
		break;
	}
	case OBS_PROPERTY_PATH:
		field = makePath(prop);
		break;
	case OBS_PROPERTY_LIST:
		field = makeList(prop);
		break;
	case OBS_PROPERTY_COLOR:
	case OBS_PROPERTY_COLOR_ALPHA: {
		const bool alpha = obs_property_get_type(prop) == OBS_PROPERTY_COLOR_ALPHA;
		auto *btn = new ColorButton(m_content, false, alpha);
		QColor c = fromObsColor(obs_data_get_int(s, key));
		if (!alpha)
			c.setAlpha(255);
		btn->setColor(c);
		connect(btn, &ColorButton::colorChanged, this, [this, prop, key, alpha](const QColor &v) {
			QColor c = v;
			if (!alpha)
				c.setAlpha(255);
			obs_data_set_int(m_settings.get(), key, toObsColor(c));
			valueChanged(prop);
		});
		field = btn;
		break;
	}
	case OBS_PROPERTY_BUTTON: {
		auto *btn = new QPushButton(desc, m_content);
		connect(btn, &QPushButton::clicked, this, [this, prop]() {
			if (obs_property_button_clicked(prop, nullptr))
				scheduleRebuild();
			emit changed();
		});
		field = btn;
		spansRow = true;
		break;
	}
	case OBS_PROPERTY_GROUP: {
		auto *box = new QGroupBox(desc, m_content);
		auto *inner = new QFormLayout(box);
		if (obs_property_group_type(prop) == OBS_GROUP_CHECKABLE) {
			box->setCheckable(true);
			box->setChecked(obs_data_get_bool(s, key));
			connect(box, &QGroupBox::toggled, this, [this, prop, key](bool v) {
				obs_data_set_bool(m_settings.get(), key, v);
				valueChanged(prop);
			});
		}
		QWidget *outer = m_content;
		m_content = box;
		addProperties(obs_property_group_content(prop), inner);
		m_content = outer;
		field = box;
		spansRow = true;
		break;
	}
	default: {
		auto *label = new QLabel(T("Properties.Unsupported"), m_content);
		label->setEnabled(false);
		field = label;
		break;
	}
	}

	field->setEnabled(field->isEnabled() && obs_property_enabled(prop));
	const QString tip = utf8(obs_property_long_description(prop));
	if (!tip.isEmpty())
		field->setToolTip(tip);

	if (spansRow)
		form->addRow(field);
	else
		form->addRow(desc, field);
	m_empty = false;
}

QWidget *PropertiesForm::makeList(obs_property_t *prop)
{
	const char *key = obs_property_name(prop);
	const obs_combo_format fmt = obs_property_list_format(prop);
	const bool editable = obs_property_list_type(prop) == OBS_COMBO_TYPE_EDITABLE;
	obs_data_t *s = m_settings.get();

	auto *combo = new QComboBox(m_content);
	combo->setEditable(editable && fmt == OBS_COMBO_FORMAT_STRING);
	combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	combo->setMinimumContentsLength(12);

	int current = -1;
	const size_t count = obs_property_list_item_count(prop);
	for (size_t i = 0; i < count; ++i) {
		QVariant value;
		bool match = false;
		switch (fmt) {
		case OBS_COMBO_FORMAT_INT: {
			const long long v = obs_property_list_item_int(prop, i);
			value = v;
			match = v == obs_data_get_int(s, key);
			break;
		}
		case OBS_COMBO_FORMAT_FLOAT: {
			const double v = obs_property_list_item_float(prop, i);
			value = v;
			match = v == obs_data_get_double(s, key);
			break;
		}
		case OBS_COMBO_FORMAT_BOOL: {
			const bool v = obs_property_list_item_bool(prop, i);
			value = v;
			match = v == obs_data_get_bool(s, key);
			break;
		}
		default: {
			const QString v = utf8(obs_property_list_item_string(prop, i));
			value = v;
			match = v == utf8(obs_data_get_string(s, key));
			break;
		}
		}
		combo->addItem(utf8(obs_property_list_item_name(prop, i)), value);
		if (obs_property_list_item_disabled(prop, i)) {
			if (auto *model = qobject_cast<QStandardItemModel *>(combo->model()))
				model->item(static_cast<int>(i))->setEnabled(false);
		}
		if (match && current < 0)
			current = static_cast<int>(i);
	}
	if (current >= 0)
		combo->setCurrentIndex(current);
	else if (combo->isEditable())
		combo->setEditText(utf8(obs_data_get_string(s, key)));
	else
		combo->setCurrentIndex(-1);

	auto apply = [this, prop, key, fmt](const QVariant &v) {
		obs_data_t *settings = m_settings.get();
		switch (fmt) {
		case OBS_COMBO_FORMAT_INT:
			obs_data_set_int(settings, key, v.toLongLong());
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			obs_data_set_double(settings, key, v.toDouble());
			break;
		case OBS_COMBO_FORMAT_BOOL:
			obs_data_set_bool(settings, key, v.toBool());
			break;
		default:
			obs_data_set_string(settings, key, v.toString().toUtf8().constData());
			break;
		}
		valueChanged(prop);
	};

	connect(combo, &QComboBox::activated, this, [combo, apply](int idx) { apply(combo->itemData(idx)); });
	if (combo->isEditable())
		connect(combo, &QComboBox::editTextChanged, this, [combo, apply](const QString &text) {
			if (combo->currentIndex() < 0 || combo->itemText(combo->currentIndex()) != text)
				apply(text);
		});
	return combo;
}

QWidget *PropertiesForm::makePath(obs_property_t *prop)
{
	const char *key = obs_property_name(prop);
	auto *row = new QWidget(m_content);
	auto *h = new QHBoxLayout(row);
	h->setContentsMargins(0, 0, 0, 0);
	auto *edit = new QLineEdit(utf8(obs_data_get_string(m_settings.get(), key)), row);
	auto *browse = new QPushButton(T("Browse"), row);
	h->addWidget(edit, 1);
	h->addWidget(browse);

	connect(edit, &QLineEdit::textEdited, this, [this, prop, key](const QString &v) {
		obs_data_set_string(m_settings.get(), key, v.toUtf8().constData());
		valueChanged(prop);
	});
	connect(browse, &QPushButton::clicked, this, [this, prop, key, edit]() {
		const QString title = utf8(obs_property_description(prop));
		const QString filter = utf8(obs_property_path_filter(prop));
		QString start = edit->text();
		if (start.isEmpty())
			start = utf8(obs_property_path_default_path(prop));
		QString path;
		switch (obs_property_path_type(prop)) {
		case OBS_PATH_DIRECTORY:
			path = QFileDialog::getExistingDirectory(this, title, start);
			break;
		case OBS_PATH_FILE_SAVE:
			path = QFileDialog::getSaveFileName(this, title, start, filter);
			break;
		default:
			path = QFileDialog::getOpenFileName(this, title, start, filter);
			break;
		}
		if (path.isEmpty())
			return;
		edit->setText(path);
		obs_data_set_string(m_settings.get(), key, path.toUtf8().constData());
		valueChanged(prop);
	});
	return row;
}

} // namespace cp

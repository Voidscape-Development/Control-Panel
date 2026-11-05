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

#include "../core/obs-util.hpp"

#include <QColor>
#include <QPushButton>
#include <QWidget>

#include <functional>

class QFormLayout;

namespace cp {

/* Stop spin boxes and combo boxes under the cursor from eating wheel events
 * while the user scrolls a form; they only react once focused. Applies to
 * all such descendants of root. */
void guardWheel(QWidget *root);

/* A swatch button with an optional "unset" state (inherit). */
class ColorButton : public QPushButton {
	Q_OBJECT

public:
	explicit ColorButton(QWidget *parent = nullptr, bool allowUnset = true, bool alpha = true);

	QColor color() const { return m_color; }
	void setColor(const QColor &color);

signals:
	void colorChanged(const QColor &color);

protected:
	void paintEvent(QPaintEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;

private:
	void pick();

	QColor m_color;
	bool m_allowUnset;
	bool m_alpha;
};

/*
 * Renders an obs_properties_t for a settings object, the way OBS's own
 * properties view does: modified callbacks, visibility and enabled state are
 * honored. Edits are written to the settings object immediately.
 *
 * Supported: bool, int, float, text, path, list, color, button, group.
 * Other property types are shown as "not supported".
 */
class PropertiesForm : public QWidget {
	Q_OBJECT

public:
	using PropertiesFn = std::function<obs_properties_t *(obs_data_t *)>;

	explicit PropertiesForm(QWidget *parent = nullptr);
	~PropertiesForm() override;

	void setSource(PropertiesFn fn, DataPtr settings);
	void clear();

	obs_data_t *settings() const { return m_settings.get(); }
	bool isEmpty() const { return m_empty; }

signals:
	void changed();

private:
	void rebuild();
	void scheduleRebuild();
	void addProperties(obs_properties_t *props, QFormLayout *form);
	void addProperty(obs_property_t *prop, QFormLayout *form);
	void valueChanged(obs_property_t *prop);

	QWidget *makeList(obs_property_t *prop);
	QWidget *makePath(obs_property_t *prop);

	PropertiesFn m_fn;
	DataPtr m_settings;
	obs_properties_t *m_props = nullptr;
	QWidget *m_content = nullptr;
	bool m_rebuildPending = false;
	bool m_empty = true;
};

} // namespace cp

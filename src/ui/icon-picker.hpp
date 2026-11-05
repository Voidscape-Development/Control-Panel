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

#include <QDialog>
#include <QTimer>

class QComboBox;
class QLabel;
class QLineEdit;
class QListView;

namespace cp {

class IconListModel;

/*
 * Searchable icon chooser: Material Symbols (thousands, searchable by name
 * and tags, filterable by category, three styles, outline/filled/filled
 * when lit) and the built-in set. Also offers an image file or no icon.
 */
class IconPickerDialog : public QDialog {
	Q_OBJECT

public:
	IconPickerDialog(const QString &currentRef, QWidget *parent = nullptr);

	/* The chosen reference; empty means "no icon". */
	QString selectedRef() const { return m_selected; }

private:
	enum Source { Material, Builtin };

	void rebuild();
	void selectRef(const QString &ref);
	void updatePreview();
	QString refForRow(int row) const;
	Source source() const;

	IconListModel *m_model;
	QComboBox *m_source;
	QLineEdit *m_search;
	QComboBox *m_category;
	QComboBox *m_style;
	QComboBox *m_fill;
	QListView *m_view;
	QLabel *m_count;
	QLabel *m_name;
	QLabel *m_previewNormal;
	QLabel *m_previewLit;
	QTimer m_debounce;
	QString m_selected;
};

} // namespace cp

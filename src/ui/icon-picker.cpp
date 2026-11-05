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

#include "icon-picker.hpp"
#include "icons.hpp"
#include "properties-form.hpp"
#include "../core/obs-util.hpp"
#include "../model/material-symbols.hpp"

#include <QAbstractListModel>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace cp {

namespace {

constexpr int kIconPx = 32;

/* Last choices, so the picker reopens where the user left off. */
struct Remembered {
	int source = 0;
	QString style = QStringLiteral("outlined");
	IconRef::Fill fill = IconRef::Fill::Outline;
};
Remembered g_last;

} // namespace

/* A flat list of references. Icons are rendered (and cached) only when the
 * view asks for a visible row. */
class IconListModel : public QAbstractListModel {
public:
	using QAbstractListModel::QAbstractListModel;

	struct Entry {
		QString ref;
		QString label;
	};

	void setEntries(QList<Entry> entries, const QColor &tint)
	{
		beginResetModel();
		m_entries = std::move(entries);
		m_tint = tint;
		endResetModel();
	}

	const QList<Entry> &entries() const { return m_entries; }

	int rowCount(const QModelIndex &parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : int(m_entries.size());
	}

	QVariant data(const QModelIndex &index, int role) const override
	{
		if (!index.isValid() || index.row() >= m_entries.size())
			return {};
		const Entry &e = m_entries[index.row()];
		switch (role) {
		case Qt::DecorationRole:
			return QIcon(icons::render(e.ref, QSize(kIconPx, kIconPx), m_dpr, m_tint));
		case Qt::ToolTipRole:
		case Qt::AccessibleTextRole:
			return e.label;
		case Qt::UserRole:
			return e.ref;
		default:
			return {};
		}
	}

	void setDevicePixelRatio(qreal dpr) { m_dpr = dpr; }

private:
	QList<Entry> m_entries;
	QColor m_tint;
	qreal m_dpr = 1.0;
};

IconPickerDialog::IconPickerDialog(const QString &currentRef, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(T("Icons.Title"));
	resize(660, 580);
	MaterialLibrary &lib = icons::material();

	auto *root = new QVBoxLayout(this);

	/* ---- filters ---- */
	auto *top = new QHBoxLayout();
	m_source = new QComboBox(this);
	m_source->addItem(T("Icons.Source.Material"), Material);
	m_source->addItem(T("Icons.Source.Builtin"), Builtin);
	m_search = new QLineEdit(this);
	m_search->setPlaceholderText(T("Icons.Search"));
	m_search->setClearButtonEnabled(true);
	m_search->setAccessibleName(T("Icons.Search"));
	m_category = new QComboBox(this);
	m_category->addItem(T("Icons.AllCategories"), QString());
	for (const QString &c : lib.categories()) {
		QString label = c;
		label.replace(QLatin1Char('&'), QStringLiteral(" & "));
		m_category->addItem(label, c);
	}
	top->addWidget(m_source);
	top->addWidget(m_search, 1);
	top->addWidget(m_category);
	root->addLayout(top);

	auto *opts = new QHBoxLayout();
	m_style = new QComboBox(this);
	m_style->addItem(T("Icons.Style.Outlined"), QStringLiteral("outlined"));
	m_style->addItem(T("Icons.Style.Rounded"), QStringLiteral("rounded"));
	m_style->addItem(T("Icons.Style.Sharp"), QStringLiteral("sharp"));
	m_fill = new QComboBox(this);
	m_fill->addItem(T("Icons.Fill.Outline"), int(IconRef::Fill::Outline));
	m_fill->addItem(T("Icons.Fill.Always"), int(IconRef::Fill::Always));
	m_fill->addItem(T("Icons.Fill.Active"), int(IconRef::Fill::Active));
	m_fill->setToolTip(T("Icons.Fill.Tip"));
	opts->addWidget(new QLabel(T("Icons.Style"), this));
	opts->addWidget(m_style);
	opts->addSpacing(12);
	opts->addWidget(new QLabel(T("Icons.Fill"), this));
	opts->addWidget(m_fill);
	opts->addStretch();
	m_count = new QLabel(this);
	m_count->setEnabled(false);
	opts->addWidget(m_count);
	root->addLayout(opts);

	/* ---- grid ---- */
	m_model = new IconListModel(this);
	m_model->setDevicePixelRatio(devicePixelRatioF());
	m_view = new QListView(this);
	m_view->setViewMode(QListView::IconMode);
	m_view->setIconSize(QSize(kIconPx, kIconPx));
	m_view->setGridSize(QSize(kIconPx + 20, kIconPx + 20));
	m_view->setUniformItemSizes(true); /* also keeps rendering to visible rows */
	m_view->setLayoutMode(QListView::Batched);
	m_view->setMovement(QListView::Static);
	m_view->setResizeMode(QListView::Adjust);
	m_view->setSelectionMode(QAbstractItemView::SingleSelection);
	m_view->setModel(m_model);
	m_view->setAccessibleName(T("Icons.Title"));
	root->addWidget(m_view, 1);

	/* ---- preview + buttons ---- */
	auto *bottom = new QHBoxLayout();
	m_previewNormal = new QLabel(this);
	m_previewLit = new QLabel(this);
	m_previewNormal->setToolTip(T("Icons.Preview.Normal"));
	m_previewLit->setToolTip(T("Icons.Preview.Lit"));
	m_name = new QLabel(this);
	m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
	bottom->addWidget(m_previewNormal);
	bottom->addWidget(m_previewLit);
	bottom->addWidget(m_name, 1);
	root->addLayout(bottom);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	QPushButton *fromFile = buttons->addButton(T("Inspector.IconFromFile"), QDialogButtonBox::ActionRole);
	QPushButton *none = buttons->addButton(T("Icons.None"), QDialogButtonBox::ActionRole);
	root->addWidget(buttons);

	if (!lib.isAvailable()) {
		/* Data files missing (e.g. a broken install): built-ins still work. */
		auto *model = qobject_cast<QStandardItemModel *>(m_source->model());
		if (model)
			model->item(0)->setEnabled(false);
		m_source->setItemText(0, T("Icons.Source.MaterialMissing"));
		g_last.source = 1;
	}

	/* ---- wiring ---- */
	m_debounce.setSingleShot(true);
	m_debounce.setInterval(150);
	connect(&m_debounce, &QTimer::timeout, this, &IconPickerDialog::rebuild);
	connect(m_search, &QLineEdit::textChanged, this, [this]() { m_debounce.start(); });
	for (QComboBox *c : {m_source, m_category, m_style, m_fill})
		connect(c, &QComboBox::activated, this, &IconPickerDialog::rebuild);
	connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &idx) {
		if (idx.isValid())
			m_selected = refForRow(idx.row());
		updatePreview();
	});
	connect(m_view, &QListView::activated, this, &QDialog::accept); /* double-click / Enter */
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(fromFile, &QPushButton::clicked, this, [this]() {
		const IconRef cur = IconRef::parse(m_selected);
		const QString path = QFileDialog::getOpenFileName(
			this, T("Inspector.Icon"), cur.kind == IconRef::Kind::File ? cur.path : QString(),
			T("Inspector.ImageFilter"));
		if (path.isEmpty())
			return;
		m_selected = IconRef::file(path).toString();
		accept();
	});
	connect(none, &QPushButton::clicked, this, [this]() {
		m_selected.clear();
		accept();
	});
	connect(this, &QDialog::accepted, this, [this]() {
		g_last.source = m_source->currentIndex();
		g_last.style = m_style->currentData().toString();
		g_last.fill = IconRef::Fill(m_fill->currentData().toInt());
	});

	/* ---- initial state ---- */
	const IconRef cur = IconRef::parse(currentRef);
	int src = g_last.source;
	QString style = g_last.style;
	IconRef::Fill fill = g_last.fill;
	if (cur.kind == IconRef::Kind::Material && lib.isAvailable()) {
		src = 0;
		style = cur.style;
		fill = cur.fill;
	} else if (cur.kind == IconRef::Kind::Builtin) {
		src = 1;
	}
	m_source->setCurrentIndex(src);
	m_style->setCurrentIndex(std::max(0, m_style->findData(style)));
	m_fill->setCurrentIndex(std::max(0, m_fill->findData(int(fill))));
	guardWheel(this);

	m_selected = currentRef;
	rebuild();
	selectRef(currentRef);
	updatePreview();
	m_search->setFocus();
}

IconPickerDialog::Source IconPickerDialog::source() const
{
	return Source(m_source->currentData().toInt());
}

QString IconPickerDialog::refForRow(int row) const
{
	return m_model->data(m_model->index(row), Qt::UserRole).toString();
}

void IconPickerDialog::rebuild()
{
	const bool material = source() == Material;
	m_category->setEnabled(material);
	m_style->setEnabled(material);
	m_fill->setEnabled(material);

	QList<IconListModel::Entry> entries;
	const QString query = m_search->text().trimmed();
	if (material) {
		MaterialLibrary &lib = icons::material();
		const QString style = m_style->currentData().toString();
		const auto fill = IconRef::Fill(m_fill->currentData().toInt());
		for (int i : lib.search(query, m_category->currentData().toString())) {
			const MaterialIcon &icon = lib.icons()[i];
			entries.append({IconRef::material(style, icon.name, fill).toString(), icon.label});
		}
	} else {
		const QStringList words = query.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
		for (const icons::BuiltinIcon &b : icons::builtins()) {
			const QString label = T(b.nameKey);
			const QString hay = (label + QLatin1Char(' ') + QLatin1String(b.id)).toLower();
			if (std::all_of(words.begin(), words.end(), [&](const QString &w) { return hay.contains(w); }))
				entries.append({IconRef::builtin(QString::fromLatin1(b.id)).toString(), label});
		}
	}

	m_model->setEntries(entries, palette().color(QPalette::ButtonText));
	m_count->setText(T("Icons.Count").arg(entries.size()));

	/* Keep the current choice selected when only style/fill changed. */
	const IconRef sel = IconRef::parse(m_selected);
	if (material && sel.kind == IconRef::Kind::Material) {
		const IconRef updated = IconRef::material(m_style->currentData().toString(), sel.name,
							  IconRef::Fill(m_fill->currentData().toInt()));
		selectRef(updated.toString());
	} else {
		selectRef(m_selected);
	}
}

void IconPickerDialog::selectRef(const QString &ref)
{
	const QList<IconListModel::Entry> &entries = m_model->entries();
	for (int i = 0; i < entries.size(); ++i) {
		if (entries[i].ref == ref) {
			const QModelIndex idx = m_model->index(i);
			m_view->setCurrentIndex(idx);
			m_view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
			m_selected = ref;
			return;
		}
	}
	m_view->clearSelection();
}

void IconPickerDialog::updatePreview()
{
	const int s = 56;
	const qreal dpr = devicePixelRatioF();
	auto swatch = [&](bool lit) {
		QPixmap pm(QSize(s, s) * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		QPainterPath path;
		path.addRoundedRect(QRectF(0.5, 0.5, s - 1, s - 1), 6, 6);
		const QPalette pal = palette();
		p.fillPath(path, lit ? pal.color(QPalette::Highlight) : pal.color(QPalette::Button));
		const QColor fg = lit ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::ButtonText);
		const QPixmap icon = icons::render(m_selected, QSize(s - 16, s - 16), dpr, fg, lit);
		if (!icon.isNull())
			p.drawPixmap(8, 8, icon);
		return pm;
	};
	m_previewNormal->setPixmap(swatch(false));
	m_previewLit->setPixmap(swatch(true));
	m_name->setText(m_selected.isEmpty() ? T("Inspector.NoIcon")
					     : QStringLiteral("<b>%1</b><br>%2")
						       .arg(icons::displayName(m_selected).toHtmlEscaped(),
							    T("Icons.Preview.Hint")));
}

} // namespace cp

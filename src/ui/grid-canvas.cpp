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

#include "grid-canvas.hpp"
#include "cell-painter.hpp"
#include "../core/obs-util.hpp"

#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace cp {

namespace {
constexpr int kHandle = 6; /* px from an edge that counts as a resize grab */
}

GridCanvas::GridCanvas(QWidget *parent) : QWidget(parent)
{
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setAccessibleName(T("Editor.Canvas"));
}

void GridCanvas::setPanel(const Panel &panel)
{
	m_panel = panel;
	if (!m_selected.isEmpty() && !m_panel.cell(m_selected))
		select(QString());
	update();
}

void GridCanvas::setSelectedCell(const QString &id)
{
	if (m_selected == id)
		return;
	m_selected = id;
	update();
}

void GridCanvas::select(const QString &id)
{
	if (m_selected == id)
		return;
	m_selected = id;
	update();
	emit selectionChanged(id);
}

grid::Metrics GridCanvas::metrics() const
{
	return grid::Metrics{size(), m_panel.rows, m_panel.cols, std::max(m_panel.theme.spacing, 2)};
}

QPoint GridCanvas::cellAt(const QPoint &px) const
{
	return grid::cellAtPixel(metrics(), px);
}

QRect GridCanvas::pixelRect(const QRect &area) const
{
	return grid::pixelRect(metrics(), area);
}

Qt::Edges GridCanvas::resizeEdgesAt(const QPoint &px, QString *cellId) const
{
	/* Prefer the selected cell so its handles win over neighbours. */
	QList<const Cell *> order;
	if (const Cell *sel = m_panel.cell(m_selected))
		order.append(sel);
	for (const Cell &c : m_panel.cells)
		if (c.id != m_selected)
			order.append(&c);

	for (const Cell *c : order) {
		const QRect r = pixelRect(c->area());
		/* Shrink the grab band on small cells so their middle still moves. */
		const int h = std::max(2, std::min(kHandle, std::min(r.width(), r.height()) / 4));
		if (!r.adjusted(-h, -h, h, h).contains(px))
			continue;
		Qt::Edges e;
		if (std::abs(px.x() - r.left()) <= h)
			e |= Qt::LeftEdge;
		if (std::abs(px.x() - r.right()) <= h)
			e |= Qt::RightEdge;
		if (std::abs(px.y() - r.top()) <= h)
			e |= Qt::TopEdge;
		if (std::abs(px.y() - r.bottom()) <= h)
			e |= Qt::BottomEdge;
		if (e) {
			*cellId = c->id;
			return e;
		}
	}
	return {};
}

void GridCanvas::updateCursor(const QPoint &px)
{
	QString id;
	const Qt::Edges e = resizeEdgesAt(px, &id);
	const bool h = e & (Qt::LeftEdge | Qt::RightEdge);
	const bool v = e & (Qt::TopEdge | Qt::BottomEdge);
	if (h && v) {
		const bool main = (e & Qt::LeftEdge && e & Qt::TopEdge) || (e & Qt::RightEdge && e & Qt::BottomEdge);
		setCursor(main ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
	} else if (h) {
		setCursor(Qt::SizeHorCursor);
	} else if (v) {
		setCursor(Qt::SizeVerCursor);
	} else {
		const QPoint c = cellAt(px);
		setCursor(grid::cellIndexAt(m_panel, c.y(), c.x()) >= 0 ? Qt::OpenHandCursor : Qt::CrossCursor);
	}
}

/* ---- painting ------------------------------------------------------- */

void GridCanvas::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const QPalette pal = palette();
	p.fillRect(rect(), pal.color(QPalette::Base));

	/* Empty slots. */
	QPen dashed(pal.color(QPalette::Mid), 1, Qt::DashLine);
	for (int r = 0; r < m_panel.rows; ++r) {
		for (int c = 0; c < m_panel.cols; ++c) {
			if (grid::cellIndexAt(m_panel, r, c) >= 0)
				continue;
			const QRect px = pixelRect(QRect(c, r, 1, 1)).adjusted(1, 1, -1, -1);
			const bool hovered = m_hoverCell == QPoint(c, r) && m_drag == Drag::None;
			p.setPen(dashed);
			/* Both branches must be QBrush: in a ternary with a QColor,
			 * Qt::NoBrush (0) silently converts to Qt::color0 (black). */
			p.setBrush(hovered ? QBrush(pal.color(QPalette::AlternateBase)) : QBrush(Qt::NoBrush));
			p.drawRoundedRect(px, m_panel.theme.cornerRadius, m_panel.theme.cornerRadius);
			if (hovered) {
				QFont f = font();
				f.setPointSize(std::clamp(std::min(px.width(), px.height()) / 4, 8, 28));
				p.setFont(f);
				p.setPen(pal.color(QPalette::PlaceholderText));
				p.drawText(px, Qt::AlignCenter, QStringLiteral("+"));
				p.setFont(font());
			}
		}
	}

	/* Cells. The one being dragged is drawn at its preview position. */
	for (const Cell &cell : m_panel.cells) {
		QRect area = cell.area();
		if ((m_drag == Drag::Move || m_drag == Drag::Resize) && cell.id == m_dragCell && m_dragMoved)
			area = m_preview;
		const QRect px = pixelRect(area);
		CellVisual v = visualFor(cell, m_panel.theme, pal);
		if (cell.kind == CellKind::Widget) {
			const QString typeName = m_widgetNames.value(cell.widgetType, cell.widgetType);
			v.text = cell.label.isEmpty() ? typeName : QStringLiteral("%1\n(%2)").arg(cell.label, typeName);
			if (!v.style.explicitBackground)
				v.style.background = pal.color(QPalette::Button).darker(110);
		} else if (cell.kind == CellKind::Label && v.text.isEmpty()) {
			v.text = T("Kind.Label");
		}
		paintCell(p, px, v, CONTROL_PANEL_STATE_NONE, false, false, font());

		if (cell.kind == CellKind::Label && !v.style.explicitBackground) {
			p.setPen(QPen(pal.color(QPalette::Mid), 1, Qt::DotLine));
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(px.adjusted(1, 1, -1, -1), m_panel.theme.cornerRadius,
					  m_panel.theme.cornerRadius);
		}

		if (cell.id == m_selected) {
			const QColor hl = pal.color(QPalette::Highlight);
			p.setPen(QPen(hl, 2));
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(px.adjusted(1, 1, -1, -1), m_panel.theme.cornerRadius,
					  m_panel.theme.cornerRadius);
			p.setPen(Qt::NoPen);
			p.setBrush(hl);
			const int h = kHandle;
			for (const QPoint &pt :
			     {px.topLeft(), px.topRight(), px.bottomLeft(), px.bottomRight(),
			      QPoint(px.center().x(), px.top()), QPoint(px.center().x(), px.bottom()),
			      QPoint(px.left(), px.center().y()), QPoint(px.right(), px.center().y())})
				p.drawRect(QRect(pt.x() - h / 2, pt.y() - h / 2, h, h));
		}
	}

	/* Create / move / resize preview. */
	if (m_drag != Drag::None && m_dragMoved && m_preview.isValid()) {
		const QColor ok(60, 180, 90);
		const QColor bad(220, 70, 70);
		const QColor c = m_previewValid ? ok : bad;
		QColor fill = c;
		fill.setAlpha(60);
		p.setPen(QPen(c, 2));
		p.setBrush(fill);
		p.drawRoundedRect(pixelRect(m_preview).adjusted(1, 1, -1, -1), m_panel.theme.cornerRadius,
				  m_panel.theme.cornerRadius);
	}

	if (m_panel.cells.isEmpty() && m_drag == Drag::None) {
		p.setPen(pal.color(QPalette::PlaceholderText));
		p.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignBottom | Qt::AlignHCenter | Qt::TextWordWrap,
			   T("Editor.CanvasHint"));
	}
}

/* ---- interaction ---------------------------------------------------- */

void GridCanvas::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton)
		return QWidget::mousePressEvent(event);

	const QPoint px = event->position().toPoint();
	m_pressPx = px;
	m_pressCell = cellAt(px);
	m_dragMoved = false;
	m_preview = QRect();

	QString id;
	m_edges = resizeEdgesAt(px, &id);
	if (m_edges) {
		m_drag = Drag::Resize;
		m_dragCell = id;
		m_origArea = m_panel.cell(id)->area();
		select(id);
		return;
	}

	const int idx = grid::cellIndexAt(m_panel, m_pressCell.y(), m_pressCell.x());
	if (idx >= 0) {
		m_drag = Drag::Move;
		m_dragCell = m_panel.cells[idx].id;
		m_origArea = m_panel.cells[idx].area();
		select(m_dragCell);
		setCursor(Qt::ClosedHandCursor);
	} else {
		m_drag = Drag::Create;
		m_dragCell.clear();
		select(QString());
	}
}

void GridCanvas::updatePreview(const QPoint &px)
{
	const QPoint cur = cellAt(px);
	switch (m_drag) {
	case Drag::Create:
		m_preview = grid::areaBetween(m_pressCell, cur);
		m_previewValid = grid::isFree(m_panel, m_preview, QString());
		break;
	case Drag::Move: {
		const QPoint delta = cur - m_pressCell;
		QRect r = m_origArea.translated(delta);
		/* Keep it inside the grid rather than refusing the move. */
		r.moveLeft(std::clamp(r.left(), 0, m_panel.cols - r.width()));
		r.moveTop(std::clamp(r.top(), 0, m_panel.rows - r.height()));
		m_preview = r;
		m_previewValid = grid::isFree(m_panel, r, m_dragCell);
		break;
	}
	case Drag::Resize: {
		int l = m_origArea.left(), t = m_origArea.top(), r = m_origArea.right(), b = m_origArea.bottom();
		if (m_edges & Qt::LeftEdge)
			l = std::min(cur.x(), r);
		if (m_edges & Qt::RightEdge)
			r = std::max(cur.x(), l);
		if (m_edges & Qt::TopEdge)
			t = std::min(cur.y(), b);
		if (m_edges & Qt::BottomEdge)
			b = std::max(cur.y(), t);
		m_preview = QRect(QPoint(l, t), QPoint(r, b));
		m_previewValid = grid::isFree(m_panel, m_preview, m_dragCell);
		break;
	}
	case Drag::None:
		break;
	}
}

void GridCanvas::mouseMoveEvent(QMouseEvent *event)
{
	const QPoint px = event->position().toPoint();
	if (m_drag == Drag::None) {
		const QPoint c = cellAt(px);
		if (c != m_hoverCell) {
			m_hoverCell = c;
			update();
		}
		updateCursor(px);
		return;
	}

	if (!m_dragMoved && (px - m_pressPx).manhattanLength() < QApplication::startDragDistance())
		return;
	m_dragMoved = true;
	updatePreview(px);
	update();
}

void GridCanvas::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton || m_drag == Drag::None)
		return QWidget::mouseReleaseEvent(event);

	const Drag drag = m_drag;
	m_drag = Drag::None;
	updateCursor(event->position().toPoint());

	if (m_dragMoved && m_previewValid) {
		if (drag == Drag::Create)
			emit createRequested(m_preview, CellKind::Button);
		else if (m_preview != m_origArea)
			emit geometryRequested(m_dragCell, m_preview);
	}
	m_dragMoved = false;
	m_preview = QRect();
	update();
}

void GridCanvas::mouseDoubleClickEvent(QMouseEvent *event)
{
	const QPoint c = cellAt(event->position().toPoint());
	const int idx = grid::cellIndexAt(m_panel, c.y(), c.x());
	if (idx >= 0)
		emit editRequested(m_panel.cells[idx].id);
	else
		emit createRequested(QRect(c, QSize(1, 1)), CellKind::Button);
}

void GridCanvas::keyPressEvent(QKeyEvent *event)
{
	const Cell *cell = m_panel.cell(m_selected);
	if (!cell)
		return QWidget::keyPressEvent(event);

	QPoint d;
	switch (event->key()) {
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
		emit deleteRequested(cell->id);
		return;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		emit editRequested(cell->id);
		return;
	case Qt::Key_D:
		if (event->modifiers() & Qt::ControlModifier) {
			emit duplicateRequested(cell->id);
			return;
		}
		break;
	case Qt::Key_Left:
		d = QPoint(-1, 0);
		break;
	case Qt::Key_Right:
		d = QPoint(1, 0);
		break;
	case Qt::Key_Up:
		d = QPoint(0, -1);
		break;
	case Qt::Key_Down:
		d = QPoint(0, 1);
		break;
	default:
		break;
	}
	if (d.isNull())
		return QWidget::keyPressEvent(event);

	QRect area = cell->area();
	if (event->modifiers() & Qt::ShiftModifier)
		area.setBottomRight(area.bottomRight() + d);
	else
		area.translate(d);
	if (area.isValid() && grid::isFree(m_panel, area, cell->id))
		emit geometryRequested(cell->id, area);
}

void GridCanvas::contextMenuEvent(QContextMenuEvent *event)
{
	const QPoint c = cellAt(event->pos());
	const int idx = grid::cellIndexAt(m_panel, c.y(), c.x());
	QMenu menu(this);
	if (idx >= 0) {
		const QString id = m_panel.cells[idx].id;
		select(id);
		menu.addAction(T("Editor.EditCell"), this, [this, id]() { emit editRequested(id); });
		menu.addAction(T("Editor.Duplicate"), this, [this, id]() { emit duplicateRequested(id); });
		menu.addSeparator();
		menu.addAction(T("Editor.DeleteCell"), this, [this, id]() { emit deleteRequested(id); });
	} else {
		const QRect area(c, QSize(1, 1));
		menu.addAction(T("Editor.AddButton"), this,
			       [this, area]() { emit createRequested(area, CellKind::Button); });
		menu.addAction(T("Editor.AddLabel"), this,
			       [this, area]() { emit createRequested(area, CellKind::Label); });
		menu.addAction(T("Editor.AddWidget"), this,
			       [this, area]() { emit createRequested(area, CellKind::Widget); });
	}
	menu.exec(event->globalPos());
}

void GridCanvas::leaveEvent(QEvent *event)
{
	m_hoverCell = QPoint(-1, -1);
	update();
	QWidget::leaveEvent(event);
}

} // namespace cp

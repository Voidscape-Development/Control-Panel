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

#include "cell-painter.hpp"
#include "icons.hpp"
#include "../core/runtime.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace cp {

ResolvedStyle resolveStyle(const Cell &cell, const Theme &theme, const QPalette &palette)
{
	const Style s = cell.style.mergedOver(theme.style);
	ResolvedStyle r;
	r.explicitBackground = s.background.isValid();
	if (cell.kind == CellKind::Label) {
		r.background = s.background.isValid() ? s.background : QColor(Qt::transparent);
		r.text = s.text.isValid() ? s.text : palette.color(QPalette::WindowText);
	} else {
		r.background = s.background.isValid() ? s.background : palette.color(QPalette::Button);
		r.text = s.text.isValid() ? s.text : palette.color(QPalette::ButtonText);
	}
	r.activeBackground = s.activeBackground.isValid() ? s.activeBackground : palette.color(QPalette::Highlight);
	r.activeText = s.activeText.isValid() ? s.activeText : palette.color(QPalette::HighlightedText);
	r.fontSize = s.fontSize;
	r.radius = theme.cornerRadius;
	return r;
}

CellVisual visualFor(const Cell &cell, const Theme &theme, const QPalette &palette)
{
	CellVisual v;
	v.kind = cell.kind;
	v.text = cell.showLabel || cell.kind == CellKind::Label ? effectiveLabel(cell) : QString();
	v.icon = cell.kind == CellKind::Button ? cell.iconPath : QString();
	v.style = resolveStyle(cell, theme, palette);
	return v;
}

namespace {

/* Largest point size at which text fits rect with word wrap. */
QFont fitFont(const QFont &base, const QString &text, const QRect &rect, int fixedSize)
{
	QFont f = base;
	if (fixedSize > 0) {
		f.setPointSize(fixedSize);
		return f;
	}
	const int flags = Qt::AlignCenter | Qt::TextWordWrap;
	int lo = 6;
	int hi = std::clamp(rect.height() / 2, 6, 22);
	while (lo < hi) {
		const int mid = (lo + hi + 1) / 2;
		f.setPointSize(mid);
		const QRect needed = QFontMetrics(f).boundingRect(rect, flags, text);
		if (needed.width() <= rect.width() && needed.height() <= rect.height())
			lo = mid;
		else
			hi = mid - 1;
	}
	f.setPointSize(lo);
	return f;
}

} // namespace

void paintCell(QPainter &p, const QRect &rect, const CellVisual &v, control_panel_state state, bool hovered,
	       bool pressed, const QFont &baseFont)
{
	p.save();
	p.setRenderHint(QPainter::Antialiasing);

	const bool active = state == CONTROL_PANEL_STATE_ON;
	const bool unavailable = state == CONTROL_PANEL_STATE_UNAVAILABLE;
	QColor bg = active ? v.style.activeBackground : v.style.background;
	const QColor fg = active ? v.style.activeText : v.style.text;

	if (v.kind == CellKind::Button) {
		if (pressed)
			bg = bg.darker(125);
		else if (hovered)
			bg = bg.lighter(115);
	}
	if (unavailable)
		p.setOpacity(0.45);

	if (bg.alpha() > 0) {
		QPainterPath path;
		path.addRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), v.style.radius, v.style.radius);
		p.fillPath(path, bg);
		/* strokePath, not drawPath: never fill with whatever brush the
		 * caller left on the painter. */
		if (v.kind == CellKind::Button && !v.style.explicitBackground && !active)
			p.strokePath(path, QPen(QColor(0, 0, 0, 60)));
	}

	QRect content = rect.adjusted(4, 3, -4, -3);
	const QPixmap icon = icons::render(
		v.icon, v.text.isEmpty() ? content.size() : QSize(content.width(), content.height() * 3 / 5),
		p.device()->devicePixelRatioF(), fg, active);
	if (!icon.isNull()) {
		QRect iconArea = content;
		if (!v.text.isEmpty())
			iconArea.setHeight(content.height() * 3 / 5);
		QRect target(QPoint(), icon.deviceIndependentSize().toSize());
		target.moveCenter(iconArea.center());
		p.drawPixmap(target.topLeft(), icon);
		if (!v.text.isEmpty())
			content.setTop(iconArea.bottom() + 1);
	}

	if (!v.text.isEmpty() && content.height() > 4) {
		QFont f = fitFont(baseFont, v.text, content, v.style.fontSize);
		if (v.kind == CellKind::Label)
			f.setBold(true);
		p.setFont(f);
		p.setPen(fg);
		p.drawText(content, Qt::AlignCenter | Qt::TextWordWrap, v.text);
	}

	if (unavailable) {
		p.setOpacity(1.0);
		const int d = std::max(6, std::min(rect.width(), rect.height()) / 8);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(220, 60, 60));
		p.drawEllipse(QRect(rect.right() - d - 3, rect.top() + 3, d, d));
	}
	p.restore();
}

/* ---- CellButton ----------------------------------------------------- */

CellButton::CellButton(QWidget *parent) : QAbstractButton(parent)
{
	setAttribute(Qt::WA_Hover);
	setFocusPolicy(Qt::NoFocus);
	setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void CellButton::setVisual(const CellVisual &visual)
{
	m_visual = visual;
	setAccessibleName(visual.text);
	update();
}

void CellButton::setState(control_panel_state state)
{
	if (state == m_state)
		return;
	m_state = state;
	update();
}

void CellButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	paintCell(p, rect(), m_visual, m_state, m_hovered, isDown(), font());
}

void CellButton::enterEvent(QEnterEvent *event)
{
	m_hovered = true;
	update();
	QAbstractButton::enterEvent(event);
}

void CellButton::leaveEvent(QEvent *event)
{
	m_hovered = false;
	update();
	QAbstractButton::leaveEvent(event);
}

} // namespace cp

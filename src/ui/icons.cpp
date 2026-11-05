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

#include "icons.hpp"
#include "../core/obs-util.hpp"

#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace cp::icons {

namespace {
const QString kPrefix = QStringLiteral("builtin:");

QString resourcePath(const QString &id)
{
	return QStringLiteral(":/control-panel/icons/%1.svg").arg(id);
}

/* Render an SVG into a transparent image fitted (aspect-preserving) and
 * centered in `px` device pixels. */
QImage renderSvg(const QString &path, const QSize &px)
{
	QSvgRenderer svg(path);
	if (!svg.isValid())
		return QImage();
	QImage img(px, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QSize fitted = svg.defaultSize().isEmpty() ? px : svg.defaultSize().scaled(px, Qt::KeepAspectRatio);
	QRect target(QPoint(), fitted);
	target.moveCenter(QRect(QPoint(), px).center());
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing);
	svg.render(&p, target);
	return img;
}
} // namespace

const QList<BuiltinIcon> &builtins()
{
	static const QList<BuiltinIcon> list = {
		{"stream", "Icon.Stream"},
		{"record", "Icon.Record"},
		{"pause", "Icon.Pause"},
		{"play", "Icon.Play"},
		{"stop", "Icon.Stop"},
		{"replay", "Icon.Replay"},
		{"save", "Icon.Save"},
		{"camera", "Icon.Camera"},
		{"screenshot", "Icon.Screenshot"},
		{"scene", "Icon.Scene"},
		{"studio", "Icon.Studio"},
		{"transition", "Icon.Transition"},
		{"filter", "Icon.Filter"},
		{"eye", "Icon.Eye"},
		{"eye-off", "Icon.EyeOff"},
		{"mic", "Icon.Mic"},
		{"mic-off", "Icon.MicOff"},
		{"speaker", "Icon.Speaker"},
		{"speaker-off", "Icon.SpeakerOff"},
		{"music", "Icon.Music"},
		{"chat", "Icon.Chat"},
		{"bell", "Icon.Bell"},
		{"clock", "Icon.Clock"},
		{"star", "Icon.Star"},
		{"heart", "Icon.Heart"},
		{"bolt", "Icon.Bolt"},
		{"flag", "Icon.Flag"},
		{"gear", "Icon.Gear"},
	};
	return list;
}

QString builtinRef(const QString &id)
{
	return kPrefix + id;
}

bool isBuiltin(const QString &ref)
{
	return ref.startsWith(kPrefix);
}

QString displayName(const QString &ref)
{
	if (isBuiltin(ref)) {
		const QString id = ref.mid(kPrefix.size());
		for (const BuiltinIcon &b : builtins())
			if (id == QLatin1String(b.id))
				return T(b.nameKey);
		return T("Missing").arg(id);
	}
	return QFileInfo(ref).fileName();
}

QPixmap render(const QString &ref, const QSize &size, qreal dpr, const QColor &tint)
{
	if (ref.isEmpty() || size.isEmpty())
		return QPixmap();
	const QSize px = size * dpr;
	const bool builtin = isBuiltin(ref);
	const QString key = QStringLiteral("cp-icon|%1|%2x%3|%4")
				    .arg(ref)
				    .arg(px.width())
				    .arg(px.height())
				    .arg(builtin ? tint.name(QColor::HexArgb) : QString());
	QPixmap cached;
	if (QPixmapCache::find(key, &cached))
		return cached;

	QImage img;
	if (builtin) {
		img = renderSvg(resourcePath(ref.mid(kPrefix.size())), px);
		if (!img.isNull()) {
			/* Keep the shape (alpha), replace the color. */
			QPainter p(&img);
			p.setCompositionMode(QPainter::CompositionMode_SourceIn);
			p.fillRect(img.rect(), tint);
		}
	} else if (ref.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
		img = renderSvg(ref, px);
	} else {
		QImage src(ref);
		if (!src.isNull())
			img = src.scaled(px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	if (img.isNull())
		return QPixmap();

	QPixmap pm = QPixmap::fromImage(img);
	pm.setDevicePixelRatio(dpr);
	QPixmapCache::insert(key, pm);
	return pm;
}

QIcon menuIcon(const QString &ref, const QColor &tint)
{
	QIcon icon;
	for (int s : {16, 24, 32, 48})
		icon.addPixmap(render(ref, QSize(s, s), 1.0, tint));
	return icon;
}

} // namespace cp::icons

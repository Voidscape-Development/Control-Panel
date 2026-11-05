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

#include "icons.hpp"
#include "../core/obs-util.hpp"
#include "../model/material-symbols.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace cp::icons {

namespace {

/* Render an SVG into a transparent image fitted (aspect-preserving) and
 * centered in `px` device pixels. */
QImage renderSvg(QSvgRenderer &svg, const QSize &px)
{
	if (!svg.isValid())
		return QImage();
	QImage img(px, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	const QSize natural = svg.defaultSize().isEmpty() ? px : svg.defaultSize();
	QRect target(QPoint(), natural.scaled(px, Qt::KeepAspectRatio));
	target.moveCenter(QRect(QPoint(), px).center());
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing);
	svg.render(&p, target);
	return img;
}

void tint(QImage &img, const QColor &color)
{
	/* Keep the shape (alpha), replace the color. */
	QPainter p(&img);
	p.setCompositionMode(QPainter::CompositionMode_SourceIn);
	p.fillRect(img.rect(), color);
}

QString styleLabel(const QString &style)
{
	if (style == QLatin1String("rounded"))
		return T("Icons.Style.Rounded");
	if (style == QLatin1String("sharp"))
		return T("Icons.Style.Sharp");
	return T("Icons.Style.Outlined");
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

MaterialLibrary &material()
{
	static MaterialLibrary lib;
	static bool opened = false;
	if (!opened) {
		opened = true;
		/* Installed next to the plugin as data files, not compiled in. */
		char *dir = obs_module_file("material-symbols");
		QString error;
		if (!dir || !lib.open(QString::fromUtf8(dir), &error))
			obs_log(LOG_WARNING, "Material Symbols unavailable (%s)",
				dir ? qUtf8Printable(error) : "data directory not found");
		else
			obs_log(LOG_INFO, "Loaded %d Material Symbols (release %s)", int(lib.icons().size()),
				qUtf8Printable(lib.version()));
		bfree(dir);
	}
	return lib;
}

QString displayName(const QString &refString)
{
	const IconRef ref = IconRef::parse(refString);
	switch (ref.kind) {
	case IconRef::Kind::Builtin:
		for (const BuiltinIcon &b : builtins())
			if (ref.name == QLatin1String(b.id))
				return T(b.nameKey);
		return T("Missing").arg(ref.name);
	case IconRef::Kind::Material: {
		const MaterialIcon *icon = material().icon(ref.name);
		QString text = icon ? icon->label : T("Missing").arg(ref.name);
		QString detail = styleLabel(ref.style);
		if (ref.fill == IconRef::Fill::Always)
			detail += QStringLiteral(", ") + T("Icons.Fill.Always").toLower();
		else if (ref.fill == IconRef::Fill::Active)
			detail += QStringLiteral(", ") + T("Icons.Fill.Active").toLower();
		return QStringLiteral("%1 (%2)").arg(text, detail);
	}
	case IconRef::Kind::File:
		return QFileInfo(ref.path).fileName();
	case IconRef::Kind::None:
		break;
	}
	return QString();
}

QPixmap render(const QString &refString, const QSize &size, qreal dpr, const QColor &color, bool active)
{
	const IconRef ref = IconRef::parse(refString);
	if (ref.kind == IconRef::Kind::None || size.isEmpty())
		return QPixmap();
	const QSize px = size * dpr;
	const bool filled = ref.kind == IconRef::Kind::Material && ref.filledWhen(active);
	const QString key = QStringLiteral("cp-icon|%1|%2|%3x%4|%5")
				    .arg(refString)
				    .arg(filled)
				    .arg(px.width())
				    .arg(px.height())
				    .arg(ref.isTinted() ? color.name(QColor::HexArgb) : QString());
	QPixmap cached;
	if (QPixmapCache::find(key, &cached))
		return cached;

	QImage img;
	switch (ref.kind) {
	case IconRef::Kind::Builtin: {
		QSvgRenderer svg(QStringLiteral(":/control-panel/icons/%1.svg").arg(ref.name));
		img = renderSvg(svg, px);
		break;
	}
	case IconRef::Kind::Material: {
		QSvgRenderer svg(material().svg(ref.style, ref.name, filled));
		img = renderSvg(svg, px);
		break;
	}
	case IconRef::Kind::File:
		if (ref.path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
			QSvgRenderer svg(ref.path);
			img = renderSvg(svg, px);
		} else {
			QImage src(ref.path);
			if (!src.isNull())
				img = src.scaled(px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}
		break;
	case IconRef::Kind::None:
		break;
	}
	if (img.isNull())
		return QPixmap();
	if (ref.isTinted())
		tint(img, color);

	QPixmap pm = QPixmap::fromImage(img);
	pm.setDevicePixelRatio(dpr);
	QPixmapCache::insert(key, pm);
	return pm;
}

QIcon menuIcon(const QString &ref, const QColor &color)
{
	QIcon icon;
	for (int s : {16, 24, 32, 48})
		icon.addPixmap(render(ref, QSize(s, s), 1.0, color));
	return icon;
}

} // namespace cp::icons

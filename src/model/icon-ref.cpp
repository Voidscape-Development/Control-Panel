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

#include "icon-ref.hpp"

namespace cp {

namespace {
const QString kBuiltin = QStringLiteral("builtin:");
const QString kMaterial = QStringLiteral("material:");
} // namespace

IconRef IconRef::parse(const QString &ref)
{
	IconRef r;
	if (ref.isEmpty())
		return r;

	if (ref.startsWith(kBuiltin)) {
		r.kind = Kind::Builtin;
		r.name = ref.mid(kBuiltin.size());
		if (r.name.isEmpty())
			r.kind = Kind::None;
		return r;
	}

	if (ref.startsWith(kMaterial)) {
		QString body = ref.mid(kMaterial.size());
		const int q = body.indexOf(QLatin1Char('?'));
		if (q >= 0) {
			const QString query = body.mid(q + 1);
			body = body.left(q);
			if (query == QLatin1String("fill=always"))
				r.fill = Fill::Always;
			else if (query == QLatin1String("fill=active"))
				r.fill = Fill::Active;
		}
		const int slash = body.indexOf(QLatin1Char('/'));
		if (slash <= 0 || slash == body.size() - 1)
			return IconRef(); /* malformed: treat as no icon */
		r.kind = Kind::Material;
		r.style = body.left(slash);
		r.name = body.mid(slash + 1);
		return r;
	}

	r.kind = Kind::File;
	r.path = ref;
	return r;
}

QString IconRef::toString() const
{
	switch (kind) {
	case Kind::Builtin:
		return kBuiltin + name;
	case Kind::Material: {
		QString s = kMaterial + style + QLatin1Char('/') + name;
		if (fill == Fill::Always)
			s += QStringLiteral("?fill=always");
		else if (fill == Fill::Active)
			s += QStringLiteral("?fill=active");
		return s;
	}
	case Kind::File:
		return path;
	case Kind::None:
		break;
	}
	return QString();
}

IconRef IconRef::builtin(const QString &id)
{
	IconRef r;
	r.kind = Kind::Builtin;
	r.name = id;
	return r;
}

IconRef IconRef::material(const QString &style, const QString &name, Fill fill)
{
	IconRef r;
	r.kind = Kind::Material;
	r.style = style;
	r.name = name;
	r.fill = fill;
	return r;
}

IconRef IconRef::file(const QString &path)
{
	IconRef r;
	r.kind = path.isEmpty() ? Kind::None : Kind::File;
	r.path = path;
	return r;
}

} // namespace cp

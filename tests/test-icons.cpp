#include "icon-ref.hpp"
#include "material-symbols.hpp"

#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QtTest>

using namespace cp;

/* The real generated packs, so the Python writer and the Qt reader are
 * tested against each other. */
static const QString kPacks = QStringLiteral(CP_SOURCE_DIR "/data/material-symbols");

class TestIcons : public QObject {
	Q_OBJECT

	MaterialLibrary lib;

private slots:
	void initTestCase()
	{
		QString error;
		QVERIFY2(lib.open(kPacks, &error), qPrintable(error));
	}

	/* ---- references ---- */

	void parseKinds()
	{
		QCOMPARE(IconRef::parse("").kind, IconRef::Kind::None);
		QCOMPARE(IconRef::parse("builtin:record").kind, IconRef::Kind::Builtin);
		QCOMPARE(IconRef::parse("builtin:record").name, QStringLiteral("record"));
		QCOMPARE(IconRef::parse("/home/me/icon.png").kind, IconRef::Kind::File);

		const IconRef m = IconRef::parse("material:rounded/mic?fill=active");
		QCOMPARE(m.kind, IconRef::Kind::Material);
		QCOMPARE(m.style, QStringLiteral("rounded"));
		QCOMPARE(m.name, QStringLiteral("mic"));
		QCOMPARE(m.fill, IconRef::Fill::Active);
		QVERIFY(!m.filledWhen(false));
		QVERIFY(m.filledWhen(true));
		QVERIFY(IconRef::parse("material:sharp/mic?fill=always").filledWhen(false));
		QVERIFY(!IconRef::parse("material:sharp/mic").filledWhen(true));
	}

	void malformedIsNone()
	{
		QCOMPARE(IconRef::parse("material:mic").kind, IconRef::Kind::None);
		QCOMPARE(IconRef::parse("material:/mic").kind, IconRef::Kind::None);
		QCOMPARE(IconRef::parse("material:outlined/").kind, IconRef::Kind::None);
		QCOMPARE(IconRef::parse("builtin:").kind, IconRef::Kind::None);
	}

	void roundTrip()
	{
		for (const char *s :
		     {"builtin:stream", "material:outlined/videocam", "material:rounded/mic?fill=always",
		      "material:sharp/star?fill=active", "C:/icons/a b.png"})
			QCOMPARE(IconRef::parse(s).toString(), QString::fromLatin1(s));
		QCOMPARE(IconRef::material("rounded", "mic", IconRef::Fill::Active).toString(),
			 QStringLiteral("material:rounded/mic?fill=active"));
	}

	/* ---- library ---- */

	void indexLoaded()
	{
		QVERIFY(lib.icons().size() > 3000);
		QVERIFY(lib.contains("mic"));
		QVERIFY(!lib.contains("definitely_not_an_icon"));
		QCOMPARE(lib.icon("fiber_manual_record")->label, QStringLiteral("Fiber manual record"));
		QVERIFY(!lib.version().isEmpty());
		/* Sorted by popularity. */
		QVERIFY(lib.icons().first().popularity >= lib.icons().last().popularity);
	}

	void categoriesAreNormalized()
	{
		const QStringList cats = lib.categories();
		QVERIFY(cats.contains("Audio&Video"));
		for (const QString &c : cats)
			QVERIFY2(c.at(0).isUpper(), qPrintable("legacy category leaked: " + c));
		/* Every icon has at least one category after legacy mapping. */
		int uncategorized = 0;
		for (const MaterialIcon &i : lib.icons())
			uncategorized += i.categories.isEmpty();
		QVERIFY(uncategorized < lib.icons().size() / 20);
	}

	void searchRanksNameFirst()
	{
		const QList<int> r = lib.search("mic");
		QVERIFY(!r.isEmpty());
		QCOMPARE(lib.icons()[r.first()].name, QStringLiteral("mic"));
	}

	void searchFindsByTag()
	{
		/* "dictation" is a tag of mic, not part of its name. */
		bool found = false;
		for (int i : lib.search("dictation"))
			found = found || lib.icons()[i].name == "mic";
		QVERIFY(found);
	}

	void searchAllWordsAndCategory()
	{
		for (int i : lib.search("arrow left"))
			QVERIFY(lib.icons()[i].haystack.contains("arrow") && lib.icons()[i].haystack.contains("left"));
		for (int i : lib.search("", "Audio&Video"))
			QVERIFY(lib.icons()[i].categories.contains("Audio&Video"));
		QCOMPARE(lib.search("").size(), lib.icons().size());
		QVERIFY(lib.search("zzzzqqq").isEmpty());
	}

	void glyphsForEveryStyleAndFill()
	{
		for (const QString &style : MaterialLibrary::styles()) {
			const QByteArray outline = lib.path(style, "mic", false);
			const QByteArray filled = lib.path(style, "mic", true);
			QVERIFY2(!outline.isEmpty() && !filled.isEmpty(), qPrintable(style));
			QVERIFY(outline != filled || style == "sharp");
		}
		QVERIFY(lib.path("outlined", "nope", false).isEmpty());
		QVERIFY(lib.path("bogus-style", "mic", false).isEmpty());
	}

	void svgRendersInk()
	{
		QSvgRenderer r(lib.svg("rounded", "videocam", true));
		QVERIFY(r.isValid());
		QImage img(48, 48, QImage::Format_ARGB32_Premultiplied);
		img.fill(Qt::transparent);
		QPainter p(&img);
		r.render(&p);
		p.end();
		int inked = 0;
		for (int y = 0; y < img.height(); ++y)
			for (int x = 0; x < img.width(); ++x)
				inked += qAlpha(img.pixel(x, y)) > 0;
		QVERIFY(inked > 48 * 48 / 10);
	}

	void missingDirectoryIsUnavailable()
	{
		MaterialLibrary empty;
		QString error;
		QVERIFY(!empty.open(QDir::temp().filePath("cp-no-such-dir"), &error));
		QVERIFY(!empty.isAvailable());
		QVERIFY(!error.isEmpty());
		QVERIFY(empty.path("outlined", "mic", false).isEmpty());
	}
};

QTEST_GUILESS_MAIN(TestIcons)
#include "test-icons.moc"

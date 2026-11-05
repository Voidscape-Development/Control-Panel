#include "grid.hpp"

#include <QtTest>

using namespace cp;

class TestGrid : public QObject {
	Q_OBJECT

private:
	static Panel panel3x3WithCenter()
	{
		Panel p;
		p.rows = 3;
		p.cols = 3;
		Cell c;
		c.id = QStringLiteral("center");
		c.row = 1;
		c.col = 1;
		p.cells.append(c);
		return p;
	}

private slots:
	void isFreeRespectsBoundsAndOverlap()
	{
		const Panel p = panel3x3WithCenter();
		QVERIFY(grid::isFree(p, QRect(0, 0, 1, 1), {}));
		QVERIFY(!grid::isFree(p, QRect(0, 0, 2, 2), {}));      /* overlaps center */
		QVERIFY(grid::isFree(p, QRect(1, 1, 2, 2), "center")); /* ignoring itself */
		QVERIFY(!grid::isFree(p, QRect(2, 2, 2, 1), {}));      /* out of bounds */
		QVERIFY(!grid::isFree(p, QRect(-1, 0, 1, 1), {}));
		QVERIFY(!grid::isFree(p, QRect(0, 0, 0, 0), {})); /* empty */
	}

	void cellIndexAt()
	{
		Panel p = panel3x3WithCenter();
		p.cells[0].colSpan = 2;
		QCOMPARE(grid::cellIndexAt(p, 1, 2), 0);
		QCOMPARE(grid::cellIndexAt(p, 0, 0), -1);
	}

	void findFreeSlotScansRowMajor()
	{
		Panel p = panel3x3WithCenter();
		QCOMPARE(*grid::findFreeSlot(p), QPoint(0, 0));
		QCOMPARE(*grid::findFreeSlot(p, 3, 1), QPoint(0, 0));
		/* Every 2x2 area of a 3x3 grid covers the center cell. */
		QVERIFY(!grid::findFreeSlot(p, 2, 2).has_value());
		QCOMPARE(*grid::findFreeSlot(p, 1, 2), QPoint(0, 0));
	}

	void findFreeSlotNoneWhenFull()
	{
		Panel p;
		p.rows = 1;
		p.cols = 1;
		Cell c;
		c.id = QStringLiteral("x");
		p.cells.append(c);
		QVERIFY(!grid::findFreeSlot(p).has_value());
	}

	void resizeClipsAndRemoves()
	{
		Panel p;
		p.rows = 4;
		p.cols = 4;
		Cell wide;
		wide.id = QStringLiteral("wide");
		wide.colSpan = 4;
		Cell far;
		far.id = QStringLiteral("far");
		far.row = 3;
		far.col = 3;
		p.cells = {wide, far};

		QCOMPARE(grid::cellsAffectedByResize(p, 2, 2), (QStringList{"wide", "far"}));
		const QStringList removed = grid::resize(p, 2, 2);
		QCOMPARE(removed, QStringList{"far"});
		QCOMPARE(p.cells.size(), 1);
		QCOMPARE(p.cells[0].colSpan, 2);
	}

	void areaBetweenNormalizes() { QCOMPARE(grid::areaBetween(QPoint(2, 3), QPoint(0, 1)), QRect(0, 1, 3, 3)); }

	void pixelRectsTileExactly()
	{
		/* 103 px wide, 3 cols, 4 px spacing: inner = 103 - 16 = 87 = 29*3 */
		grid::Metrics m{QSize(103, 50), 1, 3, 4};
		const QRect a = grid::pixelRect(m, QRect(0, 0, 1, 1));
		const QRect b = grid::pixelRect(m, QRect(1, 0, 1, 1));
		const QRect c = grid::pixelRect(m, QRect(2, 0, 1, 1));
		QCOMPARE(a.left(), 4);
		QCOMPARE(b.left(), a.right() + 1 + 4);
		QCOMPARE(c.left(), b.right() + 1 + 4);
		QCOMPARE(c.right() + 1 + 4, 103); /* trailing margin == spacing */
		QCOMPARE(a.width(), 29);
	}

	void pixelRectsDistributeRemainder()
	{
		grid::Metrics m{QSize(100, 100), 1, 3, 0};
		int total = 0;
		for (int i = 0; i < 3; ++i)
			total += grid::pixelRect(m, QRect(i, 0, 1, 1)).width();
		QCOMPARE(total, 100);
		/* A span covers its tracks plus the gaps between them. */
		QCOMPARE(grid::pixelRect(m, QRect(0, 0, 3, 1)).width(), 100);
	}

	void spanIncludesGaps()
	{
		grid::Metrics m{QSize(104, 10), 1, 4, 4};
		const QRect span = grid::pixelRect(m, QRect(1, 0, 2, 1));
		const QRect first = grid::pixelRect(m, QRect(1, 0, 1, 1));
		const QRect second = grid::pixelRect(m, QRect(2, 0, 1, 1));
		QCOMPARE(span.left(), first.left());
		QCOMPARE(span.right(), second.right());
	}

	void cellAtPixelRoundTrips()
	{
		grid::Metrics m{QSize(317, 211), 5, 7, 3};
		for (int r = 0; r < m.rows; ++r) {
			for (int c = 0; c < m.cols; ++c) {
				const QRect px = grid::pixelRect(m, QRect(c, r, 1, 1));
				QCOMPARE(grid::cellAtPixel(m, px.center()), QPoint(c, r));
				QCOMPARE(grid::cellAtPixel(m, px.topLeft()), QPoint(c, r));
				QCOMPARE(grid::cellAtPixel(m, px.bottomRight()), QPoint(c, r));
			}
		}
	}

	void cellAtPixelClamps()
	{
		grid::Metrics m{QSize(100, 100), 2, 2, 4};
		QCOMPARE(grid::cellAtPixel(m, QPoint(-50, -50)), QPoint(0, 0));
		QCOMPARE(grid::cellAtPixel(m, QPoint(500, 500)), QPoint(1, 1));
	}

	void tinySizesDoNotGoNegative()
	{
		grid::Metrics m{QSize(5, 5), 4, 4, 4};
		const QRect r = grid::pixelRect(m, QRect(3, 3, 1, 1));
		QVERIFY(r.width() >= 0);
		QVERIFY(r.height() >= 0);
	}
};

QTEST_GUILESS_MAIN(TestGrid)
#include "test-grid.moc"

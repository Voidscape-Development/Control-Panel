#include "panel-model.hpp"

#include <QtTest>

using namespace cp;

class TestModel : public QObject {
	Q_OBJECT

private:
	static Panel samplePanel()
	{
		Panel p;
		p.id = QStringLiteral("p1");
		p.name = QStringLiteral("Replay");
		p.rows = 2;
		p.cols = 3;
		p.collection = QStringLiteral("Main");
		p.showInSwitcher = false;
		p.theme.spacing = 6;
		p.theme.style.background = QColor(10, 20, 30);
		p.theme.style.fontSize = 14;

		Cell b;
		b.id = QStringLiteral("c1");
		b.label = QStringLiteral("Save");
		b.colSpan = 2;
		b.iconPath = QStringLiteral("/tmp/icon.png");
		b.style.activeBackground = QColor(200, 0, 0, 128);
		Action a1;
		a1.type = QStringLiteral("replay_save");
		Action a2;
		a2.type = QStringLiteral("scene_switch");
		a2.delayMs = 500;
		a2.settings.insert(QStringLiteral("scene"), QStringLiteral("Replay Scene"));
		b.actions = {a1, a2};
		b.indicator = 1;
		p.cells.append(b);

		Cell l;
		l.id = QStringLiteral("c2");
		l.kind = CellKind::Label;
		l.row = 1;
		l.label = QStringLiteral("Header");
		l.showLabel = true;
		p.cells.append(l);

		Cell w;
		w.id = QStringLiteral("c3");
		w.kind = CellKind::Widget;
		w.row = 1;
		w.col = 1;
		w.widgetType = QStringLiteral("status");
		w.widgetSettings.insert(QStringLiteral("kind"), QStringLiteral("clock"));
		p.cells.append(w);

		p.hotkeys.insert(QStringLiteral("show"), QJsonArray{QJsonObject{{"key", "OBS_KEY_F1"}}});
		return p;
	}

private slots:
	void roundTrip()
	{
		const Panel p = samplePanel();
		const Panel q = Panel::fromJson(p.toJson());
		QCOMPARE(q, p);
	}

	void styleMergeFillsOnlyUnset()
	{
		Style base;
		base.background = Qt::black;
		base.text = Qt::white;
		base.fontSize = 12;

		Style cell;
		cell.text = Qt::red;

		const Style r = cell.mergedOver(base);
		QCOMPARE(r.background, QColor(Qt::black));
		QCOMPARE(r.text, QColor(Qt::red));
		QCOMPARE(r.fontSize, 12);
		QVERIFY(!r.activeBackground.isValid());
	}

	void styleColorKeepsAlpha()
	{
		Style s;
		s.background = QColor(1, 2, 3, 4);
		QCOMPARE(Style::fromJson(s.toJson()).background, QColor(1, 2, 3, 4));
	}

	void emptyStyleSerializesEmpty()
	{
		QVERIFY(Style().toJson().isEmpty());
		QVERIFY(Style().isEmpty());
	}

	void invalidIndicatorFallsBackToAuto()
	{
		Cell c;
		c.actions.append(Action{QStringLiteral("x"), {}, 0});
		c.indicator = 5;
		QCOMPARE(Cell::fromJson(c.toJson()).indicator, kIndicatorAuto);
	}

	void actionsWithoutTypeAreDropped()
	{
		QJsonObject o;
		o.insert(QStringLiteral("actions"), QJsonArray{QJsonObject{{"type", ""}}, QJsonObject{{"type", "ok"}}});
		QCOMPARE(Cell::fromJson(o).actions.size(), 1);
	}

	void sanitizeClampsAndDrops()
	{
		Panel p = samplePanel();
		p.rows = 100;
		p.cols = 0;
		/* cols -> 1: c1 is clipped to one column, c2 fits, c3 starts outside. */
		QCOMPARE(sanitize(p), 1);
		QCOMPARE(p.rows, kMaxGridSize);
		QCOMPARE(p.cols, 1);
		QCOMPARE(p.cells.size(), 2);
		QCOMPARE(p.cells[0].colSpan, 1);
	}

	void sanitizeDropsOverlapsAndFixesIds()
	{
		Panel p;
		p.rows = 2;
		p.cols = 2;
		Cell a;
		a.id = QStringLiteral("same");
		Cell b = a; /* same id, same spot -> overlap, dropped */
		Cell c;
		c.id = QStringLiteral("same");
		c.col = 1; /* duplicate id, gets a fresh one */
		p.cells = {a, b, c};
		QCOMPARE(sanitize(p), 1);
		QCOMPARE(p.cells.size(), 2);
		QVERIFY(p.cells[0].id != p.cells[1].id);
		QVERIFY(!p.id.isEmpty());
	}

	void findCellByIdOrLabel()
	{
		const Panel p = samplePanel();
		QCOMPARE(p.findCell(QStringLiteral("c1"))->label, QStringLiteral("Save"));
		QCOMPARE(p.findCell(QStringLiteral("save"))->id, QStringLiteral("c1"));
		QVERIFY(!p.findCell(QStringLiteral("nope")));
	}

	void bindingMatchesCollection()
	{
		Panel p;
		QVERIFY(p.isBoundTo(QStringLiteral("Anything")));
		p.collection = QStringLiteral("Main");
		QVERIFY(p.isBoundTo(QStringLiteral("Main")));
		QVERIFY(!p.isBoundTo(QStringLiteral("Other")));
	}
};

QTEST_GUILESS_MAIN(TestModel)
#include "test-model.moc"

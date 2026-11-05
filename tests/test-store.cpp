#include "panel-store.hpp"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace cp;

class TestStore : public QObject {
	Q_OBJECT

private:
	static Panel makePanel(const QString &id, const QString &name)
	{
		Panel p;
		p.id = id;
		p.name = name;
		Cell c;
		c.id = id + QStringLiteral("-cell");
		c.label = QStringLiteral("Go");
		p.cells.append(c);
		return p;
	}

private slots:
	void missingFileIsEmptyStore()
	{
		QTemporaryDir dir;
		PanelStore store;
		QVERIFY(store.load(dir.filePath("nope.json")));
		QVERIFY(store.panels().isEmpty());
	}

	void saveAndReload()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("sub/panels.json");
		{
			PanelStore store;
			store.setAutoSave(false);
			store.load(path);
			store.addPanel(makePanel("a", "Replay"));
			store.addPanel(makePanel("b", "Filters"));
			store.setSwitcherPanel("b");
			store.setGlobalHotkeys(QJsonObject{{"next", QJsonArray{2}}});
			store.setBindingHiddenDocks({"a", "gone"});
			QVERIFY(store.saveNow());
		}
		PanelStore store;
		QVERIFY(store.load(path));
		QCOMPARE(store.panels().size(), 2);
		QCOMPARE(store.panels()[1].name, QStringLiteral("Filters"));
		QCOMPARE(store.switcherPanel(), QStringLiteral("b"));
		QCOMPARE(store.globalHotkeys().value("next").toArray().size(), 1);
		/* Ids of panels that no longer exist are dropped on load. */
		QCOMPARE(store.bindingHiddenDocks(), QStringList{"a"});
	}

	void saveKeepsBackup()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("panels.json");
		PanelStore store;
		store.setAutoSave(false);
		store.load(path);
		store.addPanel(makePanel("a", "One"));
		QVERIFY(store.saveNow());
		store.addPanel(makePanel("b", "Two"));
		QVERIFY(store.saveNow());
		QVERIFY(QFile::exists(path + ".bak"));
	}

	void corruptFileIsMovedAside()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("panels.json");
		QFile f(path);
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write("{ not json");
		f.close();

		PanelStore store;
		QString error;
		QVERIFY(!store.load(path, &error));
		QVERIFY(!error.isEmpty());
		QVERIFY(QFile::exists(path + ".invalid"));
		/* Saving afterwards works and doesn't touch the preserved file. */
		store.setAutoSave(false);
		store.addPanel(makePanel("a", "One"));
		QVERIFY(store.saveNow());
		QVERIFY(QFile::exists(path + ".invalid"));
	}

	void addPanelDeduplicatesNamesAndIds()
	{
		PanelStore store;
		store.setAutoSave(false);
		store.addPanel(makePanel("a", "Replay"));
		const QString id = store.addPanel(makePanel("a", "replay"));
		QVERIFY(id != QStringLiteral("a"));
		QCOMPARE(store.panel(id)->name, QStringLiteral("replay (2)"));
	}

	void updatePanelEmitsOnlyOnChange()
	{
		PanelStore store;
		store.setAutoSave(false);
		store.addPanel(makePanel("a", "Replay"));
		QSignalSpy spy(&store, &PanelStore::panelChanged);

		Panel p = *store.panel("a");
		store.updatePanel(p);
		QCOMPARE(spy.count(), 0);

		p.name = QStringLiteral("Renamed");
		store.updatePanel(p);
		QCOMPARE(spy.count(), 1);
		QCOMPARE(store.panel("a")->name, QStringLiteral("Renamed"));
	}

	void hotkeysSurviveEdits()
	{
		PanelStore store;
		store.setAutoSave(false);
		store.addPanel(makePanel("a", "Replay"));
		QJsonObject hk{{"show", QJsonArray{1}}};
		store.setPanelHotkeys("a", hk);

		/* The editor's copy has stale (empty) hotkeys; the store keeps its own. */
		Panel edited = makePanel("a", "Replay 2");
		store.updatePanel(edited);
		QCOMPARE(store.panel("a")->hotkeys, hk);
		store.setPanels({edited});
		QCOMPARE(store.panel("a")->hotkeys, hk);
	}

	void removeClearsSwitcherSelection()
	{
		PanelStore store;
		store.setAutoSave(false);
		store.addPanel(makePanel("a", "Replay"));
		store.setSwitcherPanel("a");
		store.removePanel("a");
		QVERIFY(store.switcherPanel().isEmpty());
	}

	void findPanelByName()
	{
		PanelStore store;
		store.setAutoSave(false);
		store.addPanel(makePanel("a", "Replay"));
		QCOMPARE(store.findPanel("REPLAY")->id, QStringLiteral("a"));
		QCOMPARE(store.findPanel("a")->id, QStringLiteral("a"));
	}

	void exportStripsHotkeys()
	{
		Panel p = makePanel("a", "Replay");
		p.hotkeys.insert("show", QJsonArray{1});
		QString error;
		const QList<Panel> parsed = PanelStore::parsePanels(PanelStore::exportPanels({p}), &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QCOMPARE(parsed.size(), 1);
		QVERIFY(parsed[0].hotkeys.isEmpty());
		QCOMPARE(parsed[0].cells.size(), 1);
	}

	void parseRejectsNewerFormat()
	{
		QString error;
		const QList<Panel> parsed = PanelStore::parsePanels(R"({"version": 99, "panels": [{}]})", &error);
		QVERIFY(parsed.isEmpty());
		QVERIFY(!error.isEmpty());
	}

	void parseAcceptsBarePanel()
	{
		QString error;
		const QList<Panel> parsed =
			PanelStore::parsePanels(R"({"name": "Solo", "rows": 2, "cols": 2})", &error);
		QCOMPARE(parsed.size(), 1);
		QCOMPARE(parsed[0].name, QStringLiteral("Solo"));
	}

	void prepareImportResolvesCollisions()
	{
		const QList<Panel> existing{makePanel("a", "Replay")};
		QList<Panel> incoming{makePanel("a", "Replay"), makePanel("b", "Replay")};
		incoming[1].cells[0].id = incoming[0].cells[0].id; /* duplicate cell id inside import */

		const QList<Panel> out = PanelStore::prepareImport(incoming, existing);
		QCOMPARE(out.size(), 2);
		QVERIFY(out[0].id != QStringLiteral("a"));
		QCOMPARE(out[1].id, QStringLiteral("b"));
		QCOMPARE(out[0].name, QStringLiteral("Replay (2)"));
		QCOMPARE(out[1].name, QStringLiteral("Replay (3)"));
		QVERIFY(out[0].cells[0].id != existing[0].cells[0].id);
		QVERIFY(out[0].cells[0].id != out[1].cells[0].id);
	}
};

QTEST_GUILESS_MAIN(TestStore)
#include "test-store.moc"

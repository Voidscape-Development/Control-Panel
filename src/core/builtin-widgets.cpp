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

#include "type-registry.hpp"

#include <obs-frontend-api.h>
#include <util/platform.h>

#include <QBoxLayout>
#include <QDateTime>
#include <QLabel>
#include <QSlider>
#include <QToolButton>

#include <cmath>

namespace cp {
namespace {

/* ===================================================================== */
/* Status display                                                        */
/* ===================================================================== */

class StatusWidget : public QWidget {
public:
	explicit StatusWidget(QWidget *parent) : QWidget(parent)
	{
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(4, 2, 4, 2);
		layout->setSpacing(0);
		m_caption = new QLabel(this);
		m_value = new QLabel(this);
		for (QLabel *l : {m_caption, m_value}) {
			l->setAlignment(Qt::AlignCenter);
			l->setAttribute(Qt::WA_TransparentForMouseEvents);
			/* Narrow cells: wrap instead of clipping, and never let the
			 * text dictate the cell's minimum size. */
			l->setWordWrap(true);
			l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		}
		/* Caption: one slightly smaller line, elided to fit (see
		 * resizeEvent) rather than wrapped and clipped in small cells. */
		m_caption->setWordWrap(false);
		QFont cf = m_caption->font();
		cf.setPointSizeF(cf.pointSizeF() * 0.85);
		m_caption->setFont(cf);
		QFont f = m_value->font();
		f.setBold(true);
		m_value->setFont(f);
		layout->addStretch();
		layout->addWidget(m_caption);
		layout->addWidget(m_value);
		layout->addStretch();
	}
	~StatusWidget() override
	{
		if (m_cpu)
			os_cpu_usage_info_destroy(m_cpu);
	}

	void configure(obs_data_t *settings)
	{
		m_kind = QString::fromUtf8(obs_data_get_string(settings, "kind"));
		QString caption = QString::fromUtf8(obs_data_get_string(settings, "caption"));
		if (caption.isEmpty())
			caption = defaultCaption(m_kind);
		m_captionText = caption;
		updateCaption();
		m_caption->setToolTip(caption);
		m_caption->setVisible(obs_data_get_bool(settings, "show_caption") && !caption.isEmpty());
		if (m_kind == QLatin1String("cpu") && !m_cpu)
			m_cpu = os_cpu_usage_info_start();
		/* No refresh() here: widgets are created during module load, before
		 * frontend APIs are usable. The first tick fills the value in. */
		if (m_value->text().isEmpty())
			m_value->setText(QStringLiteral("–"));
	}

	void refresh() { m_value->setText(valueText()); }

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		updateCaption();
	}

public:
	static QString defaultCaption(const QString &kind)
	{
		static const QHash<QString, const char *> keys = {
			{"stream_time", "Status.StreamTime"},
			{"record_time", "Status.RecordTime"},
			{"replay_status", "Status.Replay"},
			{"virtualcam_status", "Status.VirtualCam"},
			{"clock", "Status.Clock"},
			{"cpu", "Status.Cpu"},
			{"fps", "Status.Fps"},
		};
		const char *key = keys.value(kind);
		return key ? T(key) : QString();
	}

private:
	static QString formatDuration(obs_output_t *output)
	{
		if (!output || !obs_output_active(output))
			return QStringLiteral("--:--:--");
		obs_video_info ovi;
		uint64_t secs = 0;
		if (obs_get_video_info(&ovi) && ovi.fps_num > 0)
			secs = static_cast<uint64_t>(obs_output_get_total_frames(output)) * ovi.fps_den / ovi.fps_num;
		return QStringLiteral("%1:%2:%3")
			.arg(secs / 3600, 2, 10, QLatin1Char('0'))
			.arg((secs / 60) % 60, 2, 10, QLatin1Char('0'))
			.arg(secs % 60, 2, 10, QLatin1Char('0'));
	}

	static QString onOff(bool on) { return on ? T("Status.Active") : T("Status.Inactive"); }

	QString valueText()
	{
		if (m_kind == QLatin1String("stream_time")) {
			obs_output_t *out = obs_frontend_get_streaming_output();
			QString s = formatDuration(out);
			obs_output_release(out);
			return s;
		}
		if (m_kind == QLatin1String("record_time")) {
			obs_output_t *out = obs_frontend_get_recording_output();
			QString s = formatDuration(out);
			obs_output_release(out);
			if (obs_frontend_recording_paused())
				s += QStringLiteral(" ⏸");
			return s;
		}
		if (m_kind == QLatin1String("replay_status"))
			return onOff(obs_frontend_replay_buffer_active());
		if (m_kind == QLatin1String("virtualcam_status"))
			return onOff(obs_frontend_virtualcam_active());
		if (m_kind == QLatin1String("cpu"))
			return m_cpu ? QStringLiteral("%1%").arg(os_cpu_usage_info_query(m_cpu), 0, 'f', 1) : QString();
		if (m_kind == QLatin1String("fps"))
			return QString::number(obs_get_active_fps(), 'f', 1);
		/* clock */
		return QLocale().toString(QTime::currentTime(), QLocale::ShortFormat);
	}

	void updateCaption()
	{
		const int avail = std::max(0, width() - 8);
		m_caption->setText(m_caption->fontMetrics().elidedText(m_captionText, Qt::ElideRight, avail));
	}

	QLabel *m_caption;
	QString m_captionText;
	QLabel *m_value;
	QString m_kind;
	os_cpu_usage_info_t *m_cpu = nullptr;
};

class StatusWidgetType : public WidgetType {
public:
	QString id() const override { return QStringLiteral("status"); }
	QString name() const override { return T("Widget.Status"); }

	obs_properties_t *properties(obs_data_t *) const override
	{
		obs_properties_t *props = obs_properties_create();
		obs_property_t *kind = obs_properties_add_list(props, "kind", Tc("Status.Kind"), OBS_COMBO_TYPE_LIST,
							       OBS_COMBO_FORMAT_STRING);
		for (const char *k :
		     {"stream_time", "record_time", "replay_status", "virtualcam_status", "clock", "cpu", "fps"})
			obs_property_list_add_string(kind, StatusWidget::defaultCaption(k).toUtf8().constData(), k);
		obs_properties_add_bool(props, "show_caption", Tc("Status.ShowCaption"));
		obs_properties_add_text(props, "caption", Tc("Status.Caption"), OBS_TEXT_DEFAULT);
		return props;
	}
	void defaults(obs_data_t *settings) const override
	{
		obs_data_set_default_string(settings, "kind", "stream_time");
		obs_data_set_default_bool(settings, "show_caption", true);
	}
	QWidget *create(obs_data_t *settings, QWidget *parent) const override
	{
		auto *w = new StatusWidget(parent);
		w->configure(settings);
		return w;
	}
	bool update(QWidget *widget, obs_data_t *settings) const override
	{
		static_cast<StatusWidget *>(widget)->configure(settings);
		return true;
	}
	void tick(QWidget *widget, obs_data_t *) const override { static_cast<StatusWidget *>(widget)->refresh(); }
};

/* ===================================================================== */
/* Volume fader                                                          */
/* ===================================================================== */

class VolumeWidget : public QWidget {
public:
	explicit VolumeWidget(QWidget *parent) : QWidget(parent), m_fader(obs_fader_create(OBS_FADER_LOG)) {}

	~VolumeWidget() override
	{
		detach();
		obs_fader_destroy(m_fader);
	}

	void configure(obs_data_t *settings)
	{
		const bool vertical = strcmp(obs_data_get_string(settings, "orientation"), "vertical") == 0;
		const bool showMute = obs_data_get_bool(settings, "show_mute");
		if (!m_slider || vertical != m_vertical || showMute != m_showMute)
			buildUi(vertical, showMute);
		attach(obs_data_get_string(settings, "source_uuid"));
		sync();
	}

	void sync()
	{
		SourcePtr source = strongSource();
		setEnabled(source != nullptr);
		if (!source) {
			m_db->setText(T("Unavailable"));
			return;
		}
		if (!m_slider->isSliderDown()) {
			m_syncing = true;
			m_slider->setValue(static_cast<int>(std::lround(obs_fader_get_deflection(m_fader) * 1000.0f)));
			m_syncing = false;
		}
		const float db = obs_fader_get_db(m_fader);
		m_db->setText(std::isinf(db) ? QStringLiteral("-inf dB") : QStringLiteral("%1 dB").arg(db, 0, 'f', 1));
		if (m_mute) {
			const bool muted = obs_source_muted(source.get());
			m_mute->setChecked(muted);
			m_mute->setText(muted ? T("Volume.Muted") : T("Volume.Mute"));
		}
	}

private:
	SourcePtr strongSource() const { return SourcePtr(m_weak ? obs_weak_source_get_source(m_weak) : nullptr); }

	void buildUi(bool vertical, bool showMute)
	{
		qDeleteAll(findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
		delete layout();
		m_vertical = vertical;
		m_showMute = showMute;

		QBoxLayout *box = vertical ? static_cast<QBoxLayout *>(new QVBoxLayout(this))
					   : static_cast<QBoxLayout *>(new QHBoxLayout(this));
		box->setContentsMargins(6, 4, 6, 4);
		box->setSpacing(4);

		m_slider = new QSlider(vertical ? Qt::Vertical : Qt::Horizontal, this);
		m_slider->setRange(0, 1000);
		m_db = new QLabel(this);
		m_db->setAlignment(Qt::AlignCenter);
		m_db->setMinimumWidth(m_db->fontMetrics().horizontalAdvance(QStringLiteral("-00.0 dB")));
		m_mute = showMute ? new QToolButton(this) : nullptr;

		box->addWidget(m_slider, 1, Qt::AlignCenter);
		box->addWidget(m_db, 0, Qt::AlignCenter);
		if (m_mute) {
			m_mute->setCheckable(true);
			box->addWidget(m_mute, 0, Qt::AlignCenter);
			QObject::connect(m_mute, &QToolButton::clicked, this, [this](bool checked) {
				if (SourcePtr s = strongSource())
					obs_source_set_muted(s.get(), checked);
				sync();
			});
		}
		QObject::connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
			if (m_syncing)
				return;
			obs_fader_set_deflection(m_fader, v / 1000.0f);
			sync();
		});
	}

	void attach(const char *uuid)
	{
		SourcePtr current = strongSource();
		if (current && strcmp(obs_source_get_uuid(current.get()), uuid) == 0)
			return;
		detach();
		SourcePtr source = sourceByUuid(uuid);
		if (!source)
			return;
		obs_fader_attach_source(m_fader, source.get());
		m_weak = obs_source_get_weak_source(source.get());
	}

	void detach()
	{
		obs_fader_detach_source(m_fader);
		obs_weak_source_release(m_weak);
		m_weak = nullptr;
	}

	obs_fader_t *m_fader;
	obs_weak_source_t *m_weak = nullptr;
	QSlider *m_slider = nullptr;
	QLabel *m_db = nullptr;
	QToolButton *m_mute = nullptr;
	bool m_vertical = false;
	bool m_showMute = true;
	bool m_syncing = false;
};

class VolumeWidgetType : public WidgetType {
public:
	QString id() const override { return QStringLiteral("volume"); }
	QString name() const override { return T("Widget.Volume"); }

	obs_properties_t *properties(obs_data_t *settings) const override
	{
		obs_properties_t *props = obs_properties_create();
		addSourceList(props, "source_uuid", Tc("Source"), SourceFilter::Audio, settings);
		obs_property_t *o = obs_properties_add_list(props, "orientation", Tc("Volume.Orientation"),
							    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(o, Tc("Volume.Horizontal"), "horizontal");
		obs_property_list_add_string(o, Tc("Volume.Vertical"), "vertical");
		obs_properties_add_bool(props, "show_mute", Tc("Volume.ShowMute"));
		return props;
	}
	void defaults(obs_data_t *settings) const override
	{
		obs_data_set_default_string(settings, "orientation", "horizontal");
		obs_data_set_default_bool(settings, "show_mute", true);
	}
	QWidget *create(obs_data_t *settings, QWidget *parent) const override
	{
		auto *w = new VolumeWidget(parent);
		w->configure(settings);
		return w;
	}
	bool update(QWidget *widget, obs_data_t *settings) const override
	{
		static_cast<VolumeWidget *>(widget)->configure(settings);
		return true;
	}
	void tick(QWidget *widget, obs_data_t *) const override { static_cast<VolumeWidget *>(widget)->sync(); }
};

} // namespace

void registerBuiltinWidgets(TypeRegistry &reg)
{
	reg.addWidget(std::make_shared<StatusWidgetType>());
	reg.addWidget(std::make_shared<VolumeWidgetType>());
}

} // namespace cp

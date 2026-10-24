#include "AppIcon.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace PassiveImmersion {

	static QColor getStateColour(ImmersionController::State state) {

		switch (state) {
			case ImmersionController::State::Playing:		return QColor{ 0x2e, 0x9d, 0x5b };
			case ImmersionController::State::Ducked:
			case ImmersionController::State::AutoPaused:	return QColor{ 0xe0, 0x9a, 0x1f };
			case ImmersionController::State::UserPaused:	return QColor{ 0x3b, 0x7d, 0xd8 };
			case ImmersionController::State::Starting:
			case ImmersionController::State::Stopped:		break;
		}
		return QColor{ 0x80, 0x80, 0x80 };

	}

	static QPixmap draw(ImmersionController::State state, int size) {

		QPixmap pixmap{ size, size };
		pixmap.fill(Qt::transparent);

		QPainter painter{ &pixmap };
		painter.setRenderHint(QPainter::Antialiasing);

		// Background circle
		painter.setPen(Qt::NoPen);
		painter.setBrush(getStateColour(state));
		painter.drawEllipse(QRectF{ 0.0, 0.0, static_cast<qreal>(size), static_cast<qreal>(size) });

		painter.setBrush(Qt::white);
		qreal unit{ size / 16.0 };

		if (state == ImmersionController::State::Playing || state == ImmersionController::State::Ducked) {
			QPainterPath triangle{};
			triangle.moveTo(6.0 * unit, 4.5 * unit);
			triangle.lineTo(12.0 * unit, 8.0 * unit);
			triangle.lineTo(6.0 * unit, 11.5 * unit);
			triangle.closeSubpath();
			painter.drawPath(triangle);
		}
		else if (state == ImmersionController::State::AutoPaused || state == ImmersionController::State::UserPaused) {
			painter.drawRect(QRectF{ 5.0 * unit, 4.5 * unit, 2.0 * unit, 7.0 * unit });
			painter.drawRect(QRectF{ 9.0 * unit, 4.5 * unit, 2.0 * unit, 7.0 * unit });
		}
		else {
			painter.drawRect(QRectF{ 5.0 * unit, 5.0 * unit, 6.0 * unit, 6.0 * unit });
		}

		return pixmap;

	}

	QIcon AppIcon::create(ImmersionController::State state) {

		QIcon icon{};
		for (int size : { 16, 22, 32, 48, 64, 128, 256 }) {
			icon.addPixmap(draw(state, size));
		}
		return icon;

	}

}

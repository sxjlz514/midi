#include "midiinputpanel.h"
#include "midimessageformatter.h"

#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

MidiInputPanel::MidiInputPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);

    m_countLabel =
        new QLabel(QStringLiteral("消息数量：0"));

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(5000);

    m_logEdit->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
            )
        );

    mainLayout->addWidget(m_countLabel);
    mainLayout->addWidget(m_logEdit, 1);
}

MidiInputPanel::~MidiInputPanel() = default;

void MidiInputPanel::appendLog(const QString &line)
{
    m_logEdit->appendPlainText(line);
}

void MidiInputPanel::clearLog()
{
    m_logEdit->clear();
}

void MidiInputPanel::showMessage(
    quint8 status,
    quint8 data1,
    quint8 data2,
    quint32 timestampMs)
{
    m_messageCount++;

    m_countLabel->setText(
        QStringLiteral("消息数量：%1")
            .arg(m_messageCount)
        );

    m_logEdit->appendPlainText(
        MidiMessageFormatter::formatLine(
            status,
            data1,
            data2,
            timestampMs
            )
        );
}

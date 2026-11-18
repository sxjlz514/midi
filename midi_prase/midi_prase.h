#ifndef MIDI_PRASE_H
#define MIDI_PRASE_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class midi_prase;
}
QT_END_NAMESPACE

class midi_prase : public QMainWindow
{
    Q_OBJECT

public:
    explicit midi_prase(QWidget *parent = nullptr);
    ~midi_prase() override;

private:
    Ui::midi_prase *ui;
};
#endif // MIDI_PRASE_H

#include "midi_prase.h"
#include "ui_midi_prase.h"

midi_prase::midi_prase(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::midi_prase)
{
    ui->setupUi(this);
}

midi_prase::~midi_prase()
{
    delete ui;
}

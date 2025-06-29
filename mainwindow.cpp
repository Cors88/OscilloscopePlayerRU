#include "mainwindow.h"
#include "ui_mainwindow.h"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QMediaDevices>
#endif
#include <QElapsedTimer>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // перечислить аудиоустройства
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    audioDeviceList = QMediaDevices::audioOutputs();
#else
    audioDeviceList = QAudioDevice::availableDevices(QAudio::AudioOutput);
#endif
    int i = 0;
    foreach (const QAudioDevice &audioDevice, audioDeviceList)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        this->ui->comboBoxList->addItem(audioDevice.description());
#else
        this->ui->comboBoxList->addItem(audioDevice.deviceName());
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if(audioDevice == QMediaDevices::defaultAudioOutput())
#else
        if(audioDevice == QAudioDevice::defaultOutputDevice())
#endif
            this->ui->comboBoxList->setCurrentIndex(i);
        i++;
    }
    // установить максимальное количество строк в журнале
    ui->textEditInfo->document()->setMaximumBlockCount(100);
    // начальная настройка декодера
    decode.set( ui->horizontalSliderScaleX->value() <= 1000 ? ui->horizontalSliderScaleX->value() / 10 : ui->horizontalSliderScaleX->value() - 900,
                ui->horizontalSliderScaleY->value() <= 1000 ? ui->horizontalSliderScaleY->value() / 10 : ui->horizontalSliderScaleY->value() - 900,
                ui->horizontalSliderMoveX->value(),
                ui->horizontalSliderMoveY->value(),
                ui->horizontalSliderEdge->value());
    // начальная настройка осциллографа
    if (!oscilloscope.set(audioDeviceList[ui->comboBoxList->currentIndex()],
                        ui->comboBoxRate->currentText().toInt(),
                        ui->spinBoxChannel->value(),
                        ui->spinBoxChannelX->value(),
                        ui->spinBoxChannelY->value(),
                        ui->comboBoxFPS->currentText().toInt()))
    {
        QMessageBox msgBox;
        msgBox.setText("Аудиоустройство не поддерживает текущие настройки.");
        msgBox.exec();
        return;
    }
}
MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::log(const QString text)
{
    ui->textEditInfo->append(text);
}
void MainWindow::on_pushButtonOpen_clicked()
{
    QString path = QFileDialog::getOpenFileName(this,
                                                tr("Открыть"),
                                                "",
                                                tr("MPEG Video(*.mp4 *.mov *.mpg *.m4v *.avi *.flv *.rm *.rmvb);;Все файлы(*.*)"));
    if(!path.isEmpty())
    {
        log("Открыто: " + path);
        switch(decode.open(path))
        {
        case 0:
            break;
        case 1:
            QMessageBox::warning(this, "Ошибка открытия", "Не удалось открыть исходный файл.");
            return;
        case 2:
            QMessageBox::warning(this, "Ошибка открытия", "Не найдены потоковые данные.");
            return;
        default:
            QMessageBox::warning(this, "Ошибка открытия", "Неизвестная причина ошибки открытия.");
            return;
        }
        // отобразить информацию о файле
        auto fps = decode.fps();
        log("FPS: " + QString::number(double(fps.num) / double(fps.den), 'f', 2));
        auto width = decode.width();
        auto height = decode.height();
        log("Размер: " + QString::number(width) + " x " + QString::number(height));
        // установить FPS в интерфейсе
        ui->comboBoxFPS->setCurrentText(QString::number(double(fps.num) / double(fps.den), 'f', 0));    // FPS, используемый осциллографом, отличается от видео, потому что если количество точек в сцене слишком велико, необходимо более низкий FPS (на самом деле, даже если количество точек слишком велико, будет завершена одна обновление, но будут пропущены кадры, но это не так важно, поэтому настройка FPS в интерфейсе предназначена для резервирования более подходящего буфера звука)
        // установить масштаб
        int value = width > height ? 256 * 100 / width : 256 * 100 / height;
        value = value <= 100 ? value * 10 : value + 900;
        ScaleXY = true;
        ui->horizontalSliderScaleX->setValue(value);
        // установить состояние
        state = Ready;
    }
}
void MainWindow::on_pushButtonPlay_clicked()
{
    if(!decode.isReady())
    {
        QMessageBox::warning(this, "Ошибка воспроизведения", "Сначала откройте файл.");
        return;
    }
    switch (state) {
    case Inited:
        QMessageBox::warning(this, "Ошибка воспроизведения", "Сначала откройте файл.");
        return;
    case Pause:
        state = Play;
        ui->pushButtonPlay->setText("Пауза");
        return;
    case Play:
        state = Pause;
        ui->pushButtonPlay->setText("Воспроизведение");
        return;
    case Stop:
        return;
    case Ready:
        break;
    }
    // настроить буфер звука в зависимости от частоты кадров
    auto fps = decode.fps();
    int out_size = MAX_AUDIO_FRAME_SIZE*2;
    uint8_t *play_buf = nullptr;
    play_buf = reinterpret_cast<uint8_t*>(av_malloc(size_t(out_size)));
    // запустить декодер
    decode.start();
    // запустить вывод осциллографа
    oscilloscope.start();
    // таймер
    QElapsedTimer time;
    time.start();
    int i = 0;
    // установить состояние
    state = Play;
    ui->pushButtonPlay->setText("Пауза");
    while(1)
    {
        if(state == Stop)   // проверка остановки
        {
//            decode.stop();
            oscilloscope.stop();
            return;
        }
        if(1000 * double(i) * double(fps.den) / double(fps.num) < time.elapsed())
        {
            //qDebug() << double(time.elapsed()) / 1000;    // показать время
            if(state == Play)  // в режиме воспроизведения
            {
                if((!decode.video.isEmpty()) && (!decode.videoEdge.isEmpty()) && (!decode.points.isEmpty()))
                {
                    // обновить изображение видео
                    ui->videoViewer->image = decode.video.dequeue();   // установить новое изображение видео
                    ui->videoViewer->update();  // обновить изображение видео
                    ui->videoViewerEdge->image = decode.videoEdge.dequeue();   // установить новое изображение видео
                    ui->videoViewerEdge->update();  // обновить изображение видео
                    // вывести звук
                    // обновить вывод осциллографа
                    oscilloscope.setPoints(decode.points.dequeue());
                }
                else
                    log("Пропущен кадр");
            }
            i++;
        }
        //QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QCoreApplication::processEvents();
        QThread::msleep(1000 * double(fps.den) / double(fps.num) / 10);   // отдыхать на 1/10 времени между кадрами
    }
}
void MainWindow::on_pushButtonTest_clicked()
{
    if(!ui->pushButtonTest->isChecked())
    {
        oscilloscope.stop();
        ui->pushButtonTest->setText("Тестовый вывод");
        return;
    }
    // запустить вывод осциллографа
    oscilloscope.start();
    // установить волны
    QVector<Point> points(0x20);
    points[0x00].x=0x00;
    points[0x01].x=0x10;
    points[0x02].x=0x20;
    points[0x03].x=0x30;
    points[0x04].x=0x40;
    points[0x05].x=0x50;
    points[0x06].x=0x60;
    points[0x07].x=0x70;
    points[0x08].x=0x80;
    points[0x09].x=0x90;
    points[0x0a].x=0xa0;
    points[0x0b].x=0xb0;
    points[0x0c].x=0xc0;
    points[0x0d].x=0xd0;
    points[0x0e].x=0xe0;
    points[0x0f].x=0xf0;
    points[0x10].x=0xff;
    points[0x11].x=0xf0;
    points[0x12].x=0xe0;
    points[0x13].x=0xd0;
    points[0x14].x=0xc0;
    points[0x15].x=0xb0;
    points[0x16].x=0xa0;
    points[0x17].x=0x90;
    points[0x18].x=0x80;
    points[0x19].x=0x70;
    points[0x1a].x=0x60;
    points[0x1b].x=0x50;
    points[0x1c].x=0x40;
    points[0x1d].x=0x30;
    points[0x1e].x=0x20;
    points[0x1f].x=0x10;
    points[0x00].y=0x80;
    points[0x01].y=0x90;
    points[0x02].y=0xa0;
    points[0x03].y=0xb0;
    points[0x04].y=0xc0;
    points[0x05].y=0xd0;
    points[0x06].y=0xe0;
    points[0x07].y=0xf0;
    points[0x08].y=0xff;
    points[0x09].y=0xf0;
    points[0x0a].y=0xe0;
    points[0x0b].y=0xd0;
    points[0x0c].y=0xc0;
    points[0x0d].y=0xb0;
    points[0x0e].y=0xa0;
    points[0x0f].y=0x90;
    points[0x10].y=0x80;
    points[0x11].y=0x70;
    points[0x12].y=0x60;
    points[0x13].y=0x50;
    points[0x14].y=0x40;
    points[0x15].y=0x30;
    points[0x16].y=0x20;
    points[0x17].y=0x10;
    points[0x18].y=0x00;
    points[0x19].y=0x10;
    points[0x1a].y=0x20;
    points[0x1b].y=0x30;
    points[0x1c].y=0x40;
    points[0x1d].y=0x50;
    points[0x1e].y=0x60;
    points[0x1f].y=0x70;
    oscilloscope.setPoints(points);
    ui->pushButtonTest->setText("Остановить тест");
}
void MainWindow::on_comboBoxList_activated(int index)
{
    // осциллограф
    if (!oscilloscope.setAudioDevice(audioDeviceList[index]))
    {
        QMessageBox msgBox;
        msgBox.setText("Аудиоустройство не поддерживает текущие настройки.");
        msgBox.exec();
        return;
    }
}
void MainWindow::on_comboBoxRate_currentTextChanged(const QString &arg1)
{
    int rate = arg1.toInt();
    if(rate <= 0)
    {
        rate = 1;
    }
    // осциллограф
    if (!oscilloscope.setSampleRate(rate))
    {
        QMessageBox msgBox;
        msgBox.setText("Аудиоустройство не поддерживает текущие настройки.");
        msgBox.exec();
        return;
    }
}
void MainWindow::on_spinBoxChannel_valueChanged(int arg1)
{
    ui->spinBoxChannelX->setMaximum(arg1 - 1);
    ui->spinBoxChannelY->setMaximum(arg1 - 1);
    // осциллограф
    if (!oscilloscope.setChannelCount(arg1))
    {
        QMessageBox msgBox;
        msgBox.setText("Аудиоустройство не поддерживает текущие настройки.");
        msgBox.exec();
        return;
    }
}
void MainWindow::on_spinBoxChannelX_valueChanged(int arg1)
{
    // осциллограф
    oscilloscope.setChannelX(arg1);
}
void MainWindow::on_spinBoxChannelY_valueChanged(int arg1)
{
    // осциллограф
    oscilloscope.setChannelY(arg1);
}
void MainWindow::on_comboBoxFPS_currentTextChanged(const QString &arg1)
{
    int fps = arg1.toInt();
    if(fps <= 0)
    {
        fps = 1;
    }
    // осциллограф
    oscilloscope.setFPS(fps);
}
void MainWindow::on_horizontalSliderScaleX_valueChanged(int value)
{
    if(ScaleXY) ui->horizontalSliderScaleY->setValue(value);    // пропорциональное масштабирование
    value = value <= 1000 ? value / 10 : value - 900;
    decode.setScaleX(value);
    ui->labelScaleX->setText("Масштаб X: " + QString::number(value) + " %");
}
void MainWindow::on_horizontalSliderScaleY_valueChanged(int value)
{
    value = value <= 1000 ? value / 10 : value - 900;
    decode.setScaleY(value);
    ui->labelScaleY->setText("Масштаб Y: " + QString::number(value) + " %");
}
void MainWindow::on_horizontalSliderMoveX_valueChanged(int value)
{
    decode.setMoveX(value);
    ui->labelMoveX->setText("Смещение X: " + QString::number(value));
}
void MainWindow::on_horizontalSliderMoveY_valueChanged(int value)
{
    decode.setMoveY(value);
    ui->labelMoveY->setText("Смещение Y: " + QString::number(value));
}
void MainWindow::on_horizontalSliderEdge_valueChanged(int value)
{
    decode.setEdge(value);
    ui->labelEdge->setText("Порог края: " + QString::number(value));
}
void MainWindow::on_horizontalSliderScaleY_sliderReleased()
{
    ScaleXY = (ui->horizontalSliderScaleX->value() == ui->horizontalSliderScaleY->value());
}

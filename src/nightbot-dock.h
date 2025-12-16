#ifndef NIGHTBOT_DOCK_H
#define NIGHTBOT_DOCK_H

#include <QWidget>

class NightbotDock : public QWidget {
	Q_OBJECT

public:
    explicit NightbotDock(QWidget *parent = nullptr);

};

#endif // NIGHTBOT_DOCK_H
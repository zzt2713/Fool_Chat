#ifndef CLICKEDONCELABEL_H
#define CLICKEDONCELABEL_H
#include "ElaText.h"
#include <QMouseEvent>

class ClickedOnceLabel : public ElaText
{
    Q_OBJECT
public:
    explicit ClickedOnceLabel(QWidget* parent = nullptr);
    ~ClickedOnceLabel();
    virtual void mouseReleaseEvent(QMouseEvent *ev) override;

signals:
    void clicked(QString);
};

#endif // CLICKEDONCELABEL_H

#ifndef GATEWAY_H
#define GATEWAY_H

#include <QDialog>

struct GatewayPrivate;

// The Gateway's window: with a compositor it covers the screen (a scrim
// under the launcher); without one it is cut to the launcher's own shape
class Gateway : public QDialog {
        Q_OBJECT

    public:
        ~Gateway();

        static Gateway* instance();

        void show();
        void close();
        bool isOpen() const; // shown and not on its way out

    protected:
        void paintEvent(QPaintEvent* event) override;
        void changeEvent(QEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;

    private:
        explicit Gateway();

        static GatewayPrivate* d;

        void place();
        void applyShape();
};

#endif // GATEWAY_H

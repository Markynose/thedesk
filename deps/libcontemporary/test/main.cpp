#include <QtTest>

#include <tlogger.h>
#include "tst_trange.h"

int main(int argc, char* argv[]) {
    tLogWriter::setTestMode(true);

    int status = 0;
    status |= QTest::qExec(new TRange, argc, argv);

    return status;
}

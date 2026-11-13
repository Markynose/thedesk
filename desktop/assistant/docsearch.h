#ifndef DOCSEARCH_H
#define DOCSEARCH_H

#include <QString>
#include <QStringList>

// Finds documents by what's in them, not their names: PDFs (pdftotext), Word
// and LibreOffice files, plain text. Slow: call it off the GUI thread.
namespace DocSearch {
    // Every word must appear. Returns up to 10 hits, each "path · modified · …snippet…"
    QString find(const QStringList& words, const QStringList& folders);
} // namespace DocSearch

#endif // DOCSEARCH_H

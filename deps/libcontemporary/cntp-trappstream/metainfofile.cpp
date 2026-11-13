#include "metainfofile.h"

#include <QFile>
#include <QXmlStreamReader>

struct MetainfoFilePrivate {
        QXmlStreamReader reader;
        QXmlStreamWriter writer;

        QString trContents;
        QString trKey;
        QString elementNamespace;
        QString elementName;
        QString preWhitespace;
        QString leftWhitespace;
        QString rightWhitespace;
        QXmlStreamAttributes elementAttrs;

        int translationLevel = -1;
};

MetainfoFile::MetainfoFile(QString file) {
    d = new MetainfoFilePrivate();

    QFile f(file);
    f.open(QFile::ReadOnly);
    d->reader.addData(f.readAll());
    f.close();
}

MetainfoFile::~MetainfoFile() {
    delete d;
}

void MetainfoFile::setOutput(QIODevice* device) {
    d->writer.setDevice(device);
    d->writer.setAutoFormatting(true);
    d->writer.setAutoFormattingIndent(2);
}

bool MetainfoFile::moveToNext() {
    while (!d->reader.atEnd()) {
        auto token = d->reader.readNext();

        switch (token) {
            case QXmlStreamReader::NoToken:
            case QXmlStreamReader::Invalid:
                {
                    return false;
                }
            case QXmlStreamReader::StartDocument:
            case QXmlStreamReader::EndDocument:
            case QXmlStreamReader::EndElement:
            case QXmlStreamReader::Characters:
                {
                    d->preWhitespace = d->reader.text().toString();
                    // fallthrough
                }
            case QXmlStreamReader::Comment:
            case QXmlStreamReader::DTD:
            case QXmlStreamReader::EntityReference:
            case QXmlStreamReader::ProcessingInstruction:
                {
                    if (d->writer.device()) {
                        d->writer.writeCurrentToken(d->reader);
                    }
                    break;
                }
            case QXmlStreamReader::StartElement:
                {
                    if (d->writer.device()) {
                        d->writer.writeStartElement(d->reader.namespaceUri().toString(), d->reader.name().toString());
                    }

                    // Check if this is translatable!
                    bool translatableAttributeFound = false;
                    for (const auto& attr : d->reader.attributes()) {
                        if (attr.prefix().toString() != "cntp") {
                            if (d->writer.device()) {
                                d->writer.writeAttribute(attr);
                            }
                        } else {
                            if (attr.name().toString() == "trkey") {
                                // Stop! This is translatable!
                                translatableAttributeFound = true;
                                d->trKey = attr.value().toString();

                                // Keep processing other attributes and we'll figure this out at the end
                            }
                        }
                    }

                    if (translatableAttributeFound) {
                        d->elementNamespace = d->reader.namespaceUri().toString();
                        d->elementName = d->reader.name().toString();
                        d->elementAttrs = d->reader.attributes();
                        d->trContents = d->reader.readElementText();

                        // Calculate the whitespace
                        for (auto i = 0; i < d->trContents.length(); i++) {
                            if (!d->trContents.at(i).isSpace()) {
                                d->leftWhitespace = d->trContents.left(i);
                                d->trContents = d->trContents.mid(i);
                                break;
                            }
                        }
                        for (auto i = d->trContents.length() - 1; i >= 0; i--) {
                            if (!d->trContents.at(i).isSpace()) {
                                d->rightWhitespace = d->trContents.mid(i + 1);
                                d->trContents = d->trContents.left(i + 1);
                                break;
                            }
                        }
                        for (auto i = d->preWhitespace.length() - 1; i >= 0; i--) {
                            if (d->preWhitespace.at(i) == '\n') {
                                d->preWhitespace = d->preWhitespace.mid(i);
                                break;
                            }
                        }

                        // Read past the end element tag in preparation for possible translations
                        d->reader.readNext();
                        if (d->writer.device()) {
                            d->writer.writeCharacters(d->leftWhitespace);
                            d->writer.writeCharacters(d->trContents);
                            d->writer.writeCharacters(d->rightWhitespace);
                            d->writer.writeEndElement();
                        }

                        return true;
                    }

                    d->preWhitespace = "";

                    break;
                }
        }
    }
    return false;
}

QString MetainfoFile::trKey() {
    return d->trKey;
}

QString MetainfoFile::trContents() {
    return d->trContents;
}

void MetainfoFile::writeTranslation(QString language, QString contents) {
    d->writer.writeCharacters(d->preWhitespace);
    d->writer.writeStartElement(d->elementNamespace, d->elementName);

    // Check if this is translatable!
    bool translatableAttributeFound = false;
    for (const auto& attr : d->elementAttrs) {
        if (attr.prefix().toString() != "cntp") {
            d->writer.writeAttribute(attr);
        }
    }

    d->writer.writeAttribute("xml:lang", language);

    d->writer.writeCharacters(d->leftWhitespace);
    d->writer.writeCharacters(contents);
    d->writer.writeCharacters(d->rightWhitespace);
    d->writer.writeEndElement();
}

bool MetainfoFile::isError() {
    return d->reader.hasError();
}

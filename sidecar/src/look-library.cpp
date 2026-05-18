#include "look-library.h"
#include "template-manager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

LookLibrary &LookLibrary::instance()
{
    static LookLibrary s;
    return s;
}

void LookLibrary::loadBuiltIn()
{
    if (m_loaded) return;
    m_loaded = true;

    const auto &tm = TemplateManager::instance();

    QDir dir(":/looks/data/looks");
    const auto entries = dir.entryList(QStringList() << "*.json", QDir::Files,
                                       QDir::Name);
    for (const QString &name : entries) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly)) continue;

        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) continue;

        Look l = Look::fromJson(doc.object());
        if (!l.tmpl.isValid()) {
            if (const auto *t = tm.findById(l.templateId))
                l.tmpl = *t;
        }
        if (!l.isValid()) continue;
        m_looks.append(l);
    }
}

const Look *LookLibrary::findById(const QString &id) const
{
    const auto look = std::find_if(m_looks.begin(), m_looks.end(),
        [&id](const Look &l) { return l.id == id; });
    return look != m_looks.end() ? &(*look) : nullptr;
}

QStringList LookLibrary::categories() const
{
    QStringList out;
    QSet<QString> seen;
    for (const auto &l : m_looks) {
        const QString c = l.category.isEmpty() ? QStringLiteral("General")
                                               : l.category;
        if (!seen.contains(c)) { seen.insert(c); out.append(c); }
    }
    return out;
}

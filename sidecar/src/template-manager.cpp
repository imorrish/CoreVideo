#include "template-manager.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

TemplateManager &TemplateManager::instance()
{
    static TemplateManager inst;
    return inst;
}

void TemplateManager::loadBuiltIn()
{
    static const char *builtIn[] = {
        ":/templates/data/templates/1-up.json",
        ":/templates/data/templates/2-up-sbs.json",
        ":/templates/data/templates/2-up-pip.json",
        ":/templates/data/templates/3-up-feature.json",
        ":/templates/data/templates/4-up-grid.json",
        ":/templates/data/templates/5-up-feature.json",
        ":/templates/data/templates/6-up-grid.json",
        ":/templates/data/templates/7-up-feature.json",
        ":/templates/data/templates/8-up-grid.json",
        ":/templates/data/templates/speaker-screenshare.json",
        ":/templates/data/templates/talk-show.json",
        nullptr
    };
    for (int i = 0; builtIn[i]; ++i)
        loadFile(builtIn[i]);
    emit templatesChanged();
}

void TemplateManager::loadDirectory(const QString &path)
{
    const QDir dir(path);
    for (const QString &f : dir.entryList({"*.json"}, QDir::Files))
        loadFile(dir.filePath(f));
    emit templatesChanged();
}

void TemplateManager::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    LayoutTemplate t = LayoutTemplate::fromJson(obj);
    if (!t.isValid()) return;

    // Replace existing if same id, otherwise append
    const auto existing = std::find_if(m_templates.begin(), m_templates.end(),
        [&t](const LayoutTemplate &tmpl) {
            return tmpl.id == t.id;
        });
    if (existing != m_templates.end()) {
        *existing = t;
        return;
    }
    m_templates.append(t);
}

const LayoutTemplate *TemplateManager::findById(const QString &id) const
{
    const auto tmpl = std::find_if(m_templates.begin(), m_templates.end(),
        [&id](const LayoutTemplate &t) {
            return t.id == id;
        });
    return tmpl != m_templates.end() ? &(*tmpl) : nullptr;
}

bool TemplateManager::save(const LayoutTemplate &tmpl, const QString &dir)
{
    QDir().mkpath(dir);
    QFile f(dir + "/" + tmpl.id + ".json");
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(tmpl.toJson()).toJson());
    return true;
}

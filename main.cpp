#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHttpMultiPart>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kLimit = 2000;          // message characters
constexpr int kEmbedLimit = 4096;     // embed description characters
constexpr int kMaxFiles = 10;         // attachments per message
const QStringList kAllowedPrefixes = {
    QStringLiteral("https://discord.com/api/webhooks/"),
    QStringLiteral("https://discordapp.com/api/webhooks/"),
    QStringLiteral("https://canary.discord.com/api/webhooks/"),
    QStringLiteral("https://ptb.discord.com/api/webhooks/"),
};

bool isAllowedUrl(const QString &url)
{
    for (const QString &prefix : kAllowedPrefixes) {
        if (url.startsWith(prefix))
            return true;
    }
    return false;
}

bool isHttpUrl(const QString &url)
{
    return url.startsWith("https://") || url.startsWith("http://");
}

QSettings settings() { return QSettings("PigeonBuild", "WebhookSender"); }
} // namespace

class Window : public QWidget
{
public:
    Window()
    {
        setWindowTitle("Webhook Sender");
        resize(640, 720);

        // --- saved webhooks -------------------------------------------------------------------
        m_saved = new QComboBox(this);
        auto *saveBtn = new QPushButton("Save", this);
        auto *deleteBtn = new QPushButton("Delete", this);
        saveBtn->setToolTip("Save the URL, display name and avatar under a name");
        deleteBtn->setToolTip("Remove the selected saved webhook from this PC");
        auto *savedRow = new QHBoxLayout;
        savedRow->addWidget(m_saved, 1);
        savedRow->addWidget(saveBtn);
        savedRow->addWidget(deleteBtn);

        m_url = new QLineEdit(this);
        m_url->setEchoMode(QLineEdit::Password);
        m_url->setPlaceholderText("https://discord.com/api/webhooks/...");
        auto *showUrl = new QPushButton("Show", this);
        showUrl->setCheckable(true);
        auto *urlRow = new QHBoxLayout;
        urlRow->addWidget(m_url, 1);
        urlRow->addWidget(showUrl);

        // --- identity: name + avatar ----------------------------------------------------------
        m_name = new QLineEdit(this);
        m_name->setPlaceholderText("optional");
        m_avatar = new QLineEdit(this);
        m_avatar->setPlaceholderText("optional image link (https://...png / .jpg / .gif)");
        m_preview = new QLabel(this);
        m_preview->setFixedSize(48, 48);
        m_preview->setScaledContents(true);
        m_preview->setFrameShape(QFrame::StyledPanel);
        auto *avatarRow = new QHBoxLayout;
        avatarRow->addWidget(m_avatar, 1);
        avatarRow->addWidget(m_preview);
        auto *applyBtn = new QPushButton("Set webhook name + picture from file...", this);
        applyBtn->setToolTip("Changes the webhook's own default name and picture in Discord (PATCH)");

        auto *form = new QFormLayout;
        form->addRow("Saved:", savedRow);
        form->addRow("Webhook URL:", urlRow);
        form->addRow("Display name:", m_name);
        form->addRow("Avatar URL:", avatarRow);
        form->addRow("", applyBtn);

        // --- message + attachments ------------------------------------------------------------
        m_text = new QPlainTextEdit(this);
        m_text->setPlaceholderText("Message (Markdown works: **bold**, *italic*, `code`, > quote)");
        m_count = new QLabel(this);
        m_files = new QLabel("No files", this);
        auto *attachBtn = new QPushButton("Attach files...", this);
        auto *clearFilesBtn = new QPushButton("Clear", this);
        auto *filesRow = new QHBoxLayout;
        filesRow->addWidget(attachBtn);
        filesRow->addWidget(clearFilesBtn);
        filesRow->addWidget(m_files, 1);

        // --- embed ------------------------------------------------------------------------------
        m_embed = new QGroupBox("Embed", this);
        m_embed->setCheckable(true);
        m_embed->setChecked(false);
        m_embedTitle = new QLineEdit(m_embed);
        m_embedDesc = new QPlainTextEdit(m_embed);
        m_embedDesc->setMaximumHeight(90);
        m_embedImage = new QLineEdit(m_embed);
        m_embedImage->setPlaceholderText("optional image link");
        m_colorBtn = new QPushButton(m_embed);
        auto *embedForm = new QFormLayout(m_embed);
        embedForm->addRow("Title:", m_embedTitle);
        embedForm->addRow("Text:", m_embedDesc);
        embedForm->addRow("Image URL:", m_embedImage);
        embedForm->addRow("Color:", m_colorBtn);
        setColor(QColor(0x58, 0x65, 0xF2));

        m_send = new QPushButton("Send", this);
        m_send->setDefault(true);
        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto *bottom = new QHBoxLayout;
        bottom->addWidget(m_count);
        bottom->addStretch();
        bottom->addWidget(m_send);

        auto *root = new QVBoxLayout(this);
        root->addLayout(form);
        root->addWidget(new QLabel("Message:", this));
        root->addWidget(m_text, 1);
        root->addLayout(filesRow);
        root->addWidget(m_embed);
        root->addLayout(bottom);
        root->addWidget(m_status);

        // --- wiring -----------------------------------------------------------------------------
        connect(m_text, &QPlainTextEdit::textChanged, this, [this] { updateState(); });
        connect(m_embedDesc, &QPlainTextEdit::textChanged, this, [this] { updateState(); });
        connect(m_embedTitle, &QLineEdit::textChanged, this, [this] { updateState(); });
        connect(m_embed, &QGroupBox::toggled, this, [this] { updateState(); });
        connect(m_send, &QPushButton::clicked, this, [this] { send(); });
        connect(showUrl, &QPushButton::toggled, this, [this, showUrl](bool on) {
            m_url->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
            showUrl->setText(on ? "Hide" : "Show");
        });
        connect(m_avatar, &QLineEdit::editingFinished, this, [this] { loadPreview(); });
        connect(saveBtn, &QPushButton::clicked, this, [this] { saveCurrent(); });
        connect(deleteBtn, &QPushButton::clicked, this, [this] { deleteCurrent(); });
        connect(m_saved, &QComboBox::activated, this, [this](int i) { loadSaved(i); });
        connect(attachBtn, &QPushButton::clicked, this, [this] { pickFiles(); });
        connect(clearFilesBtn, &QPushButton::clicked, this, [this] { m_attachments.clear(); updateFiles(); });
        connect(m_colorBtn, &QPushButton::clicked, this, [this] {
            const QColor c = QColorDialog::getColor(m_color, this, "Embed color");
            if (c.isValid())
                setColor(c);
        });
        connect(applyBtn, &QPushButton::clicked, this, [this] { applyWebhookIdentity(); });

        // Ctrl+Enter (main and keypad Enter) sends.
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this, [this] { send(); });
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this, [this] { send(); });

        // last used values + saved list
        QSettings s = settings();
        m_url->setText(s.value("webhook/url").toString());
        m_name->setText(s.value("webhook/name").toString());
        m_avatar->setText(s.value("webhook/avatar").toString());
        reloadSaved(s.value("webhook/selected").toString());
        loadPreview();
        updateState();
    }

private:
    // Count Unicode code points, which is what Discord counts.
    static int codePoints(const QString &s) { return s.toUcs4().size(); }

    // ---------------------------------------------------------------------------------------------
    // Saved webhooks: JSON array [{label, url, name, avatar}] in QSettings (this PC only)
    // ---------------------------------------------------------------------------------------------
    QJsonArray savedList() const
    {
        return QJsonDocument::fromJson(settings().value("saved").toByteArray()).array();
    }

    void writeSaved(const QJsonArray &list)
    {
        settings().setValue("saved", QJsonDocument(list).toJson(QJsonDocument::Compact));
    }

    void reloadSaved(const QString &select)
    {
        m_saved->clear();
        m_saved->addItem("(not saved)");
        const QJsonArray list = savedList();
        for (const QJsonValue &v : list)
            m_saved->addItem(v.toObject()["label"].toString());
        const int i = m_saved->findText(select);
        m_saved->setCurrentIndex(i > 0 ? i : 0);
    }

    void loadSaved(int index)
    {
        if (index <= 0)
            return;
        const QJsonObject o = savedList().at(index - 1).toObject();
        m_url->setText(o["url"].toString());
        m_name->setText(o["name"].toString());
        m_avatar->setText(o["avatar"].toString());
        settings().setValue("webhook/selected", o["label"].toString());
        loadPreview();
        showStatus("Loaded \"" + o["label"].toString() + "\"", false);
    }

    void saveCurrent()
    {
        const QString url = m_url->text().trimmed();
        if (!isAllowedUrl(url)) {
            showStatus("Error: paste a valid Discord webhook URL first", true);
            return;
        }
        bool ok = false;
        const QString current = m_saved->currentIndex() > 0 ? m_saved->currentText() : QString();
        const QString label = QInputDialog::getText(this, "Save webhook", "Name for this webhook:",
                                                    QLineEdit::Normal, current, &ok).trimmed();
        if (!ok || label.isEmpty())
            return;
        QJsonArray list = savedList();
        QJsonObject o{{"label", label}, {"url", url}, {"name", m_name->text().trimmed()},
                      {"avatar", m_avatar->text().trimmed()}};
        bool replaced = false;
        for (int i = 0; i < list.size(); ++i) {
            if (list.at(i).toObject()["label"].toString() == label) {
                list.replace(i, o);
                replaced = true;
            }
        }
        if (!replaced)
            list.append(o);
        writeSaved(list);
        settings().setValue("webhook/selected", label);
        reloadSaved(label);
        showStatus("Saved \"" + label + "\"", false);
    }

    void deleteCurrent()
    {
        const int index = m_saved->currentIndex();
        if (index <= 0)
            return;
        const QString label = m_saved->currentText();
        if (QMessageBox::question(this, "Delete", "Remove \"" + label + "\" from the saved webhooks?\n"
                                  "(The webhook itself stays in Discord.)") != QMessageBox::Yes)
            return;
        QJsonArray list = savedList();
        list.removeAt(index - 1);
        writeSaved(list);
        reloadSaved(QString());
        showStatus("Removed \"" + label + "\"", false);
    }

    // ---------------------------------------------------------------------------------------------
    // Avatar preview, attachments, embed color
    // ---------------------------------------------------------------------------------------------
    void loadPreview()
    {
        const QString url = m_avatar->text().trimmed();
        m_preview->clear();
        if (!isHttpUrl(url))
            return;
        QNetworkReply *reply = m_nam.get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            QPixmap pix;
            if (reply->error() == QNetworkReply::NoError && pix.loadFromData(reply->readAll()))
                m_preview->setPixmap(pix);
        });
    }

    void pickFiles()
    {
        const QStringList picked = QFileDialog::getOpenFileNames(this, "Attach files");
        for (const QString &f : picked) {
            if (m_attachments.size() >= kMaxFiles)
                break;
            if (!m_attachments.contains(f))
                m_attachments.append(f);
        }
        updateFiles();
    }

    void updateFiles()
    {
        if (m_attachments.isEmpty()) {
            m_files->setText("No files");
        } else {
            QStringList names;
            for (const QString &f : m_attachments)
                names << QFileInfo(f).fileName();
            m_files->setText(QStringLiteral("%1 file(s): %2").arg(m_attachments.size()).arg(names.join(", ")));
        }
        updateState();
    }

    void setColor(const QColor &c)
    {
        m_color = c;
        m_colorBtn->setText(c.name().toUpper());
        m_colorBtn->setStyleSheet(QStringLiteral("background:%1; color:%2;")
                                      .arg(c.name(), c.lightness() > 140 ? "black" : "white"));
    }

    bool embedHasContent() const
    {
        return m_embed->isChecked() && (!m_embedTitle->text().trimmed().isEmpty()
                                        || !m_embedDesc->toPlainText().trimmed().isEmpty()
                                        || isHttpUrl(m_embedImage->text().trimmed()));
    }

    void updateState()
    {
        const int n = codePoints(m_text->toPlainText());
        const int e = codePoints(m_embedDesc->toPlainText());
        m_count->setText(m_embed->isChecked() ? QStringLiteral("%1 / %2  ·  embed %3 / %4").arg(n).arg(kLimit).arg(e).arg(kEmbedLimit)
                                              : QStringLiteral("%1 / %2").arg(n).arg(kLimit));
        const bool tooLong = n > kLimit || (m_embed->isChecked() && e > kEmbedLimit);
        QPalette pal = m_count->palette();
        pal.setColor(QPalette::WindowText, tooLong ? Qt::red : palette().color(QPalette::WindowText));
        m_count->setPalette(pal);

        const bool hasSomething = n > 0 || !m_attachments.isEmpty() || embedHasContent();
        m_send->setEnabled(!m_busy && hasSomething && !tooLong);
    }

    void showStatus(const QString &text, bool error)
    {
        QPalette pal = m_status->palette();
        pal.setColor(QPalette::WindowText, error ? Qt::red : palette().color(QPalette::WindowText));
        m_status->setPalette(pal);
        m_status->setText(text);
    }

    // ---------------------------------------------------------------------------------------------
    // Sending
    // ---------------------------------------------------------------------------------------------
    QJsonObject payload() const
    {
        QJsonObject body;
        const QString text = m_text->toPlainText();
        if (!text.isEmpty())
            body["content"] = text;
        const QString name = m_name->text().trimmed();
        if (!name.isEmpty())
            body["username"] = name;
        const QString avatar = m_avatar->text().trimmed();
        if (isHttpUrl(avatar))
            body["avatar_url"] = avatar;
        if (embedHasContent()) {
            QJsonObject embed;
            if (!m_embedTitle->text().trimmed().isEmpty())
                embed["title"] = m_embedTitle->text().trimmed();
            if (!m_embedDesc->toPlainText().trimmed().isEmpty())
                embed["description"] = m_embedDesc->toPlainText();
            embed["color"] = static_cast<int>(m_color.rgb() & 0xFFFFFF);
            const QString image = m_embedImage->text().trimmed();
            if (isHttpUrl(image))
                embed["image"] = QJsonObject{{"url", image}};
            body["embeds"] = QJsonArray{embed};
        }
        body["allowed_mentions"] = QJsonObject{{"parse", QJsonArray()}};   // no pings
        return body;
    }

    void send()
    {
        if (!m_send->isEnabled())
            return;

        const QString url = m_url->text().trimmed();
        if (!isAllowedUrl(url)) {
            showStatus("Error: URL must be a Discord webhook (https://discord.com/api/webhooks/...)", true);
            return;
        }
        QSettings s = settings();
        s.setValue("webhook/url", url);
        s.setValue("webhook/name", m_name->text().trimmed());
        s.setValue("webhook/avatar", m_avatar->text().trimmed());

        const QByteArray json = QJsonDocument(payload()).toJson(QJsonDocument::Compact);
        QNetworkRequest request{QUrl(url)};
        QNetworkReply *reply = nullptr;

        if (m_attachments.isEmpty()) {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            reply = m_nam.post(request, json);
        } else {
            auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
            QHttpPart jsonPart;
            jsonPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"payload_json\"");
            jsonPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            jsonPart.setBody(json);
            multi->append(jsonPart);
            QMimeDatabase mimes;
            for (int i = 0; i < m_attachments.size(); ++i) {
                auto *file = new QFile(m_attachments.at(i), multi);
                if (!file->open(QIODevice::ReadOnly)) {
                    delete multi;
                    showStatus("Error: cannot read " + m_attachments.at(i), true);
                    return;
                }
                QHttpPart part;
                const QString fileName = QFileInfo(*file).fileName().replace('"', '_');
                part.setHeader(QNetworkRequest::ContentDispositionHeader,
                               QStringLiteral("form-data; name=\"files[%1]\"; filename=\"%2\"").arg(i).arg(fileName));
                part.setHeader(QNetworkRequest::ContentTypeHeader, mimes.mimeTypeForFile(m_attachments.at(i)).name());
                part.setBodyDevice(file);
                multi->append(part);
            }
            reply = m_nam.post(request, multi);
            multi->setParent(reply);
        }

        m_busy = true;
        updateState();
        showStatus("Sending...", false);
        connect(reply, &QNetworkReply::finished, this, [this, reply] { onSent(reply); });
    }

    void onSent(QNetworkReply *reply)
    {
        reply->deleteLater();
        m_busy = false;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code >= 200 && code < 300) {
            m_text->clear();
            m_attachments.clear();
            updateFiles();
            showStatus("Sent", false);
        } else {
            showStatus(errorText(reply, code), true);
        }
        updateState();
    }

    static QString errorText(QNetworkReply *reply, int code)
    {
        QString detail = reply->errorString();
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        if (json.contains("message"))
            detail = json["message"].toString();
        return code == 0 ? "Error: " + detail : QStringLiteral("Error HTTP %1: %2").arg(code).arg(detail);
    }

    // Changes the webhook's own default name and picture (PATCH /webhooks/{id}/{token}).
    void applyWebhookIdentity()
    {
        const QString url = m_url->text().trimmed();
        if (!isAllowedUrl(url)) {
            showStatus("Error: paste a valid Discord webhook URL first", true);
            return;
        }
        const QString path = QFileDialog::getOpenFileName(this, "Webhook picture (leave empty to keep it)", QString(),
                                                          "Images (*.png *.jpg *.jpeg *.gif)");
        QJsonObject body;
        const QString name = m_name->text().trimmed();
        if (!name.isEmpty())
            body["name"] = name;
        if (!path.isEmpty()) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
                showStatus("Error: picture missing or larger than 8 MB", true);
                return;
            }
            const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
            body["avatar"] = QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(file.readAll().toBase64()));
        }
        if (body.isEmpty()) {
            showStatus("Nothing to change: type a display name or pick a picture", true);
            return;
        }
        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *reply = m_nam.sendCustomRequest(request, "PATCH", QJsonDocument(body).toJson(QJsonDocument::Compact));
        showStatus("Updating the webhook...", false);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            showStatus(code >= 200 && code < 300 ? "Webhook name/picture updated" : errorText(reply, code),
                       !(code >= 200 && code < 300));
        });
    }

    QComboBox *m_saved = nullptr;
    QLineEdit *m_url = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_avatar = nullptr;
    QLabel *m_preview = nullptr;
    QPlainTextEdit *m_text = nullptr;
    QLabel *m_files = nullptr;
    QStringList m_attachments;
    QGroupBox *m_embed = nullptr;
    QLineEdit *m_embedTitle = nullptr;
    QPlainTextEdit *m_embedDesc = nullptr;
    QLineEdit *m_embedImage = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QColor m_color;
    QLabel *m_count = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_send = nullptr;
    QNetworkAccessManager m_nam;
    bool m_busy = false;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Window window;
    window.show();
    return app.exec();
}

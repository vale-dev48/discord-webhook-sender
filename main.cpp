#include <QApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kLimit = 2000;
const QStringList kAllowedPrefixes = {
    QStringLiteral("https://discord.com/api/webhooks/"),
    QStringLiteral("https://discordapp.com/api/webhooks/"),
};

bool isAllowedUrl(const QString &url)
{
    for (const QString &prefix : kAllowedPrefixes) {
        if (url.startsWith(prefix))
            return true;
    }
    return false;
}
} // namespace

class Window : public QWidget
{
public:
    Window()
    {
        setWindowTitle("Webhook Sender");
        resize(560, 460);

        m_url = new QLineEdit(this);
        m_url->setEchoMode(QLineEdit::Password);
        m_url->setPlaceholderText("https://discord.com/api/webhooks/...");
        m_url->setText(QSettings("PigeonBuild", "WebhookSender").value("webhook/url").toString());

        m_name = new QLineEdit(this);
        m_name->setPlaceholderText("optional");

        m_text = new QPlainTextEdit(this);
        m_count = new QLabel(this);
        m_send = new QPushButton("Send", this);
        m_status = new QLabel(this);
        m_status->setWordWrap(true);

        auto *form = new QFormLayout;
        form->addRow("Webhook URL:", m_url);
        form->addRow("Display name:", m_name);

        auto *bottom = new QHBoxLayout;
        bottom->addWidget(m_count);
        bottom->addStretch();
        bottom->addWidget(m_send);

        auto *root = new QVBoxLayout(this);
        root->addLayout(form);
        root->addWidget(new QLabel("Message:", this));
        root->addWidget(m_text, 1);
        root->addLayout(bottom);
        root->addWidget(m_status);

        connect(m_text, &QPlainTextEdit::textChanged, this, [this] { updateState(); });
        connect(m_send, &QPushButton::clicked, this, [this] { send(); });

        // Ctrl+Enter (main and keypad Enter) sends.
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this, [this] { send(); });
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this, [this] { send(); });

        updateState();
    }

private:
    // Count Unicode code points, which is what Discord counts.
    static int codePoints(const QString &s) { return s.toUcs4().size(); }

    void updateState()
    {
        const int n = codePoints(m_text->toPlainText());
        m_count->setText(QStringLiteral("%1 / %2").arg(n).arg(kLimit));

        QPalette pal = m_count->palette();
        pal.setColor(QPalette::WindowText, n > kLimit ? Qt::red : palette().color(QPalette::WindowText));
        m_count->setPalette(pal);

        m_send->setEnabled(!m_busy && n > 0 && n <= kLimit);
    }

    void showStatus(const QString &text, bool error)
    {
        QPalette pal = m_status->palette();
        pal.setColor(QPalette::WindowText, error ? Qt::red : palette().color(QPalette::WindowText));
        m_status->setPalette(pal);
        m_status->setText(text);
    }

    void send()
    {
        if (!m_send->isEnabled())
            return;

        const QString url = m_url->text().trimmed();
        if (!isAllowedUrl(url)) {
            showStatus("Error: URL must start with https://discord.com/api/webhooks/ "
                       "or https://discordapp.com/api/webhooks/", true);
            return;
        }

        QSettings("PigeonBuild", "WebhookSender").setValue("webhook/url", url);

        QJsonObject body;
        body["content"] = m_text->toPlainText();
        const QString name = m_name->text().trimmed();
        if (!name.isEmpty())
            body["username"] = name;
        QJsonObject mentions;
        mentions["parse"] = QJsonArray();
        body["allowed_mentions"] = mentions;

        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        m_busy = true;
        updateState();
        showStatus("Sending...", false);

        QNetworkReply *reply = m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply] { onFinished(reply); });
    }

    void onFinished(QNetworkReply *reply)
    {
        reply->deleteLater();
        m_busy = false;

        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code >= 200 && code < 300) {
            m_text->clear();
            showStatus("Sent", false);
        } else {
            QString detail = reply->errorString();
            const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
            if (json.contains("message"))
                detail = json["message"].toString();
            if (code == 0)
                showStatus("Error: " + detail, true);
            else
                showStatus(QStringLiteral("Error HTTP %1: %2").arg(code).arg(detail), true);
        }
        updateState();
    }

    QLineEdit *m_url = nullptr;
    QLineEdit *m_name = nullptr;
    QPlainTextEdit *m_text = nullptr;
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

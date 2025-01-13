#ifndef CHATMESSAGEMODEL_H
#define CHATMESSAGEMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include "llama.h"

// This model holds and manages chat messages for display in a ListView or similar view.
// It wraps llama_chat_message structures into a Qt-friendly model.
class ChatMessageModel : public QAbstractListModel {
    Q_OBJECT

public:
    // Constructor: creates an empty chat message model.
    explicit ChatMessageModel(QObject *parent = nullptr);
    ~ChatMessageModel() override;

    // Required overrides for QAbstractListModel.
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Appends multiple llama_chat_message objects at once.
    void append(const std::vector<llama_chat_message> &messages);

    // Appends a single message with a sender and content.
    // Returns the index of the appended item.
    int appendSingle(const QString &sender, const QString &content);

    // Updates the content of a message at a given row index.
    void updateMessageContent(int row, const QString &newContent);

private:
    // Custom roles to map sender and content into QML (or other view).
    enum Role {
        Sender = Qt::UserRole + 1,
        MessageContent,
    };

    // Stores all messages in a vector.
    std::vector<llama_chat_message> m_messages;
};

#endif // CHATMESSAGEMODEL_H

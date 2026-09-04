#ifndef QREST_DOCUMENT_H
#define QREST_DOCUMENT_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#ifndef Q_MOC_RUN
#include "data_packet.hpp"
#include "file_header.hpp"
#include "metadata.hpp"
#endif

class QrestDocument {
public:
    enum class Mode {
        View,
        EditDraft,
        NewDraft,
    };

    QrestDocument();

    void newDocument();
    void openFile(const QString &fileUrl);
    void beginEdit();
    void saveAs(const QString &fileUrl);

    [[nodiscard]] Mode mode() const noexcept;
    [[nodiscard]] bool isDirty() const noexcept;
    [[nodiscard]] bool canModify() const noexcept;
    [[nodiscard]] QString sourcePath() const;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] QByteArray rawFileBytes() const;

#ifndef Q_MOC_RUN
    [[nodiscard]] const qrest_data::FileHeader &fileHeader() const noexcept;
    [[nodiscard]] const qrest_data::Metadata &metadata() const noexcept;
    [[nodiscard]] const qrest_data::DataPacket &dataPacket() const noexcept;

    void replaceMetadata(const qrest_data::Metadata &metadata);
    void replaceDataPacket(const qrest_data::DataPacket &packet);
    void replaceContent(const qrest_data::Metadata &metadata,
                        const qrest_data::DataPacket &packet);
    [[nodiscard]] bool validate(QStringList *errors = nullptr) const;
#endif

private:
    void setDirty(bool dirty) noexcept;
    void synchronizeHeader();
    void refreshRawBytes();
    [[nodiscard]] QByteArray serialize() const;

    Mode m_mode{Mode::NewDraft};
    QString m_sourcePath;
    bool m_dirty{false};
    QByteArray m_rawFileBytes;

#ifndef Q_MOC_RUN
    qrest_data::FileHeader m_fileHeader;
    qrest_data::Metadata m_metadata;
    qrest_data::DataPacket m_dataPacket;
    QByteArray m_originalBytes;
#endif
};

#endif // QREST_DOCUMENT_H

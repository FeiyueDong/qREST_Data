#ifndef QREST_VIEW_MODEL_H
#define QREST_VIEW_MODEL_H

#include <QAbstractTableModel> // 新增：用于高性能表格显示
#include <QByteArray>
#include <QFutureWatcher>
#include <QItemSelectionModel>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QtQml>
#include <QVariantList>
#include <QVariantMap>

#include "qrest_document.h"

#ifndef Q_MOC_RUN
#include "../core/external_import.hpp"
#include "data_packet.hpp"
#include "file_header.hpp"
#include "metadata.hpp"
#endif

#ifndef Q_MOC_RUN
struct ExternalImportResult {
    QString format;
    QString path;
    qrest_data::tools::ExternalDataset dataset;
};
#endif

// =========================================================
// 新增：专为 QML TableView 设计的高性能数据模型
// =========================================================
class DataTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit DataTableModel(QObject *parent = nullptr);

    // 必须重写的三个核心函数
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    // 用于提供表头名称 (如 Channel 1, Channel 2)
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // 自定义方法：加载数据包指针并刷新表格
#ifndef Q_MOC_RUN
    void loadData(const qrest_data::DataPacket *packet);
#endif
    void clear();

private:
#ifndef Q_MOC_RUN
    const qrest_data::DataPacket *m_packet = nullptr;
#endif
};

class ChannelTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ChannelTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

#ifndef Q_MOC_RUN
    void loadMetadata(const qrest_data::Metadata *metadata);
#endif
    void clear();

private:
#ifndef Q_MOC_RUN
    const qrest_data::Metadata *m_metadata = nullptr;
#endif
};

class ValidationTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum class Severity {
        Info,
        Warning,
        Error,
    };

    struct Issue {
        Severity severity{Severity::Info};
        QString area;
        QString message;
    };

    explicit ValidationTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void setIssues(QList<Issue> issues);
    void clear();
    [[nodiscard]] int errorCount() const;
    [[nodiscard]] int warningCount() const;
    [[nodiscard]] int infoCount() const;

private:
    QList<Issue> m_issues;
};

class BinaryTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit BinaryTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void loadBytes(const QByteArray &bytes);
    void clear();
    [[nodiscard]] int byteCount() const;

private:
    QByteArray m_bytes;
};


// =========================================================
// 主控制器
// =========================================================
class QrestViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString headerMagic READ headerMagic NOTIFY headerUpdated)
    Q_PROPERTY(int metadataSize READ metadataSize NOTIFY headerUpdated)
    Q_PROPERTY(int dataSize READ dataSize NOTIFY headerUpdated)
    Q_PROPERTY(QJsonObject metadataJson READ metadataJson WRITE setMetadataJson
                   NOTIFY metadataUpdated)

    // 数据包头部信息属性
    Q_PROPERTY(int packetSourceId READ packetSourceId NOTIFY packetUpdated)
    Q_PROPERTY(
        int packetChannelCount READ packetChannelCount NOTIFY packetUpdated)
    Q_PROPERTY(
        int packetSamplingRate READ packetSamplingRate NOTIFY packetUpdated)
    Q_PROPERTY(
        int packetDataPointCount READ packetDataPointCount NOTIFY packetUpdated)
    Q_PROPERTY(
        QString documentModeName READ documentModeName NOTIFY documentUpdated)
    Q_PROPERTY(
        QString documentStatus READ documentStatus NOTIFY documentUpdated)
    Q_PROPERTY(
        QString sourceFileName READ sourceFileName NOTIFY documentUpdated)
    Q_PROPERTY(bool isDirty READ isDirty NOTIFY documentUpdated)
    Q_PROPERTY(bool canModify READ canModify NOTIFY documentUpdated)
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY documentUpdated)
    Q_PROPERTY(bool canSaveAs READ canSaveAs NOTIFY documentUpdated)
    Q_PROPERTY(
        QString metadataHeader READ metadataHeader NOTIFY metadataUpdated)
    Q_PROPERTY(QString metadataVersionText READ metadataVersionText NOTIFY
                   metadataUpdated)
    Q_PROPERTY(QString distanceUnit READ distanceUnit NOTIFY metadataUpdated)
    Q_PROPERTY(QString timeUnit READ timeUnit NOTIFY metadataUpdated)
    Q_PROPERTY(QString projectName READ projectName NOTIFY metadataUpdated)
    Q_PROPERTY(
        QString structuralType READ structuralType NOTIFY metadataUpdated)
    Q_PROPERTY(double longitude READ longitude NOTIFY metadataUpdated)
    Q_PROPERTY(double latitude READ latitude NOTIFY metadataUpdated)
    Q_PROPERTY(double northAngle READ northAngle NOTIFY metadataUpdated)
    Q_PROPERTY(
        QString footprintShape READ footprintShape NOTIFY metadataUpdated)
    Q_PROPERTY(
        double footprintLength READ footprintLength NOTIFY metadataUpdated)
    Q_PROPERTY(double footprintWidth READ footprintWidth NOTIFY metadataUpdated)
    Q_PROPERTY(
        double footprintRadius READ footprintRadius NOTIFY metadataUpdated)
    Q_PROPERTY(
        QString boundingBoxText READ boundingBoxText NOTIFY metadataUpdated)
    Q_PROPERTY(QString polygonCornersText READ polygonCornersText NOTIFY
                   metadataUpdated)
    Q_PROPERTY(QString elevationText READ elevationText NOTIFY metadataUpdated)
    Q_PROPERTY(
        QString elevationSummary READ elevationSummary NOTIFY metadataUpdated)
    Q_PROPERTY(int elevationNum READ elevationNum NOTIFY metadataUpdated)
    Q_PROPERTY(QString provider READ provider NOTIFY metadataUpdated)
    Q_PROPERTY(int channelNum READ channelNum NOTIFY metadataUpdated)
    Q_PROPERTY(QString eventName READ eventName NOTIFY metadataUpdated)
    Q_PROPERTY(QString startTime READ startTime NOTIFY metadataUpdated)
    Q_PROPERTY(
        qlonglong startTimestamp READ startTimestamp NOTIFY metadataUpdated)
    Q_PROPERTY(int samplingRate READ samplingRate NOTIFY metadataUpdated)
    Q_PROPERTY(QString samplingIntervalText READ samplingIntervalText NOTIFY
                   metadataUpdated)
    Q_PROPERTY(int dataNpts READ dataNpts NOTIFY metadataUpdated)
    Q_PROPERTY(QString corrected READ corrected NOTIFY metadataUpdated)
    // 暴露给 QML 表格使用的模型
    Q_PROPERTY(QAbstractTableModel *tableModel READ tableModel CONSTANT)
    Q_PROPERTY(QAbstractTableModel *channelModel READ channelModel CONSTANT)
    Q_PROPERTY(QItemSelectionModel *channelSelectionModel READ
                   channelSelectionModel CONSTANT)
    Q_PROPERTY(int selectedChannelRow READ selectedChannelRow NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(bool hasSelectedChannel READ hasSelectedChannel NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(bool canEditChannelOrder READ canEditChannelOrder NOTIFY
                   documentUpdated)
    Q_PROPERTY(int selectedChannelNo READ selectedChannelNo NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(QString selectedChannelId READ selectedChannelId NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(QString selectedChannelMeasurand READ selectedChannelMeasurand
                   NOTIFY selectedChannelUpdated)
    Q_PROPERTY(QString selectedChannelDeviceType READ selectedChannelDeviceType
                   NOTIFY selectedChannelUpdated)
    Q_PROPERTY(double selectedChannelScale READ selectedChannelScale NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(double selectedChannelAzimuth READ selectedChannelAzimuth NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(QString selectedChannelDirection READ selectedChannelDirection
                   NOTIFY selectedChannelUpdated)
    Q_PROPERTY(double selectedChannelX READ selectedChannelX NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(double selectedChannelY READ selectedChannelY NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(double selectedChannelZ READ selectedChannelZ NOTIFY
                   selectedChannelUpdated)
    Q_PROPERTY(QVariantList geometryFloorOutlines READ geometryFloorOutlines
                   NOTIFY geometryUpdated)
    Q_PROPERTY(QVariantList sensorLayoutPoints READ sensorLayoutPoints NOTIFY
                   geometryUpdated)
    Q_PROPERTY(
        QVariantList structureEdges READ structureEdges NOTIFY geometryUpdated)
    Q_PROPERTY(QVariantList structureSensors READ structureSensors NOTIFY
                   geometryUpdated)
    Q_PROPERTY(
        QVariantList structureAxes READ structureAxes NOTIFY geometryUpdated)
    Q_PROPERTY(QVariantMap structureViewBounds READ structureViewBounds NOTIFY
                   geometryUpdated)
    Q_PROPERTY(
        QString geometrySummary READ geometrySummary NOTIFY geometryUpdated)
    Q_PROPERTY(
        QAbstractTableModel *validationModel READ validationModel CONSTANT)
    Q_PROPERTY(int validationErrorCount READ validationErrorCount NOTIFY
                   validationUpdated)
    Q_PROPERTY(int validationWarningCount READ validationWarningCount NOTIFY
                   validationUpdated)
    Q_PROPERTY(int validationInfoCount READ validationInfoCount NOTIFY
                   validationUpdated)
    Q_PROPERTY(QString validationStatusText READ validationStatusText NOTIFY
                   validationUpdated)
    Q_PROPERTY(QAbstractTableModel *binaryModel READ binaryModel CONSTANT)
    Q_PROPERTY(int binaryByteCount READ binaryByteCount NOTIFY fileLoaded)
    Q_PROPERTY(QString binarySummary READ binarySummary NOTIFY fileLoaded)
    Q_PROPERTY(bool externalImportLoading READ externalImportLoading NOTIFY
                   externalImportUpdated)
    Q_PROPERTY(bool externalImportReady READ externalImportReady NOTIFY
                   externalImportUpdated)
    Q_PROPERTY(QString externalImportFormat READ externalImportFormat NOTIFY
                   externalImportUpdated)
    Q_PROPERTY(QString externalImportPath READ externalImportPath NOTIFY
                   externalImportUpdated)
    Q_PROPERTY(int externalImportChannelCount READ externalImportChannelCount
                   NOTIFY externalImportUpdated)
    Q_PROPERTY(int externalImportSampleCount READ externalImportSampleCount
                   NOTIFY externalImportUpdated)
    Q_PROPERTY(double externalImportSampleRate READ externalImportSampleRate
                   NOTIFY externalImportUpdated)
    Q_PROPERTY(QString externalImportStatus READ externalImportStatus NOTIFY
                   externalImportUpdated)
    Q_PROPERTY(QVariantList externalImportSourceChannels READ
                   externalImportSourceChannels NOTIFY externalImportUpdated)
    Q_PROPERTY(QVariantList externalImportTargetChannels READ
                   externalImportTargetChannels NOTIFY externalImportUpdated)
    // 供 QML 表格使用的选择模型
    Q_PROPERTY(QItemSelectionModel *selectionModel READ selectionModel CONSTANT)
    Q_PROPERTY(
        int packetDataEncodings READ packetDataEncodings NOTIFY packetUpdated)
    Q_PROPERTY(
        qlonglong packetTimestamp READ packetTimestamp NOTIFY packetUpdated)

public:
    explicit QrestViewModel(QObject *parent = nullptr);

    QString headerMagic() const;
    int metadataSize() const;
    int dataSize() const;
    QJsonObject metadataJson() const;
    void setMetadataJson(const QJsonObject &json);
    // 数据包 Getter
    QItemSelectionModel *selectionModel() const;
    int packetSourceId() const;
    int packetChannelCount() const;
    int packetSamplingRate() const;
    int packetDataPointCount() const;
    QAbstractTableModel *tableModel() const;
    QAbstractTableModel *channelModel() const;
    QItemSelectionModel *channelSelectionModel() const;
    int packetDataEncodings() const;
    qlonglong packetTimestamp() const;
    QString documentModeName() const;
    QString documentStatus() const;
    QString sourceFileName() const;
    bool isDirty() const;
    bool canModify() const;
    bool canEdit() const;
    bool canSaveAs() const;
    QString metadataHeader() const;
    QString metadataVersionText() const;
    QString distanceUnit() const;
    QString timeUnit() const;
    QString projectName() const;
    QString structuralType() const;
    double longitude() const;
    double latitude() const;
    double northAngle() const;
    QString footprintShape() const;
    double footprintLength() const;
    double footprintWidth() const;
    double footprintRadius() const;
    QString boundingBoxText() const;
    QString polygonCornersText() const;
    QString elevationText() const;
    QString elevationSummary() const;
    int elevationNum() const;
    QString provider() const;
    int channelNum() const;
    QString eventName() const;
    QString startTime() const;
    qlonglong startTimestamp() const;
    int samplingRate() const;
    QString samplingIntervalText() const;
    int dataNpts() const;
    QString corrected() const;
    int selectedChannelRow() const;
    bool hasSelectedChannel() const;
    bool canEditChannelOrder() const;
    int selectedChannelNo() const;
    QString selectedChannelId() const;
    QString selectedChannelDeviceType() const;
    QString selectedChannelMeasurand() const;
    double selectedChannelScale() const;
    double selectedChannelAzimuth() const;
    QString selectedChannelDirection() const;
    double selectedChannelX() const;
    double selectedChannelY() const;
    double selectedChannelZ() const;
    QVariantList geometryFloorOutlines() const;
    QVariantList sensorLayoutPoints() const;
    QVariantList structureEdges() const;
    QVariantList structureSensors() const;
    QVariantList structureAxes() const;
    QVariantMap structureViewBounds() const;
    QString geometrySummary() const;
    QAbstractTableModel *validationModel() const;
    int validationErrorCount() const;
    int validationWarningCount() const;
    int validationInfoCount() const;
    QString validationStatusText() const;
    QAbstractTableModel *binaryModel() const;
    int binaryByteCount() const;
    QString binarySummary() const;
    bool externalImportLoading() const;
    bool externalImportReady() const;
    QString externalImportFormat() const;
    QString externalImportPath() const;
    int externalImportChannelCount() const;
    int externalImportSampleCount() const;
    double externalImportSampleRate() const;
    QString externalImportStatus() const;
    QVariantList externalImportSourceChannels() const;
    QVariantList externalImportTargetChannels() const;
    Q_INVOKABLE int binaryRowForOffset(int offset) const;
    Q_INVOKABLE int findBinaryAscii(const QString &text, int startRow) const;
    Q_INVOKABLE int findBinaryHex(const QString &hex, int startRow) const;

    Q_INVOKABLE void newFile();
    Q_INVOKABLE void openFile(const QString &fileUrl);
    Q_INVOKABLE void saveFile(const QString &fileUrl);
    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void validateDocument();
    Q_INVOKABLE void runValidationReport();
    Q_INVOKABLE void updateUnits(const QString &distanceUnit,
                                 const QString &timeUnit);
    Q_INVOKABLE void updateBuildingBasic(const QString &projectName,
                                         const QString &structuralType);
    Q_INVOKABLE void
    updateGeoLocation(double longitude, double latitude, double northAngle);
    Q_INVOKABLE void updateFootprint(const QString &shape,
                                     double length,
                                     double width,
                                     double radius);
    Q_INVOKABLE void updatePolygonCornersText(const QString &text);
    Q_INVOKABLE void updateElevationText(const QString &text);
    Q_INVOKABLE void updateProvider(const QString &provider);
    Q_INVOKABLE void updateDataInfo(const QString &eventName,
                                    int samplingRate,
                                    int npts,
                                    const QString &corrected);
    Q_INVOKABLE void updateStartTimestamp(qlonglong timestamp);
    Q_INVOKABLE void selectChannel(int row);
    Q_INVOKABLE void addChannel();
    Q_INVOKABLE void duplicateSelectedChannel();
    Q_INVOKABLE void deleteSelectedChannel();
    Q_INVOKABLE void updateSelectedChannel(const QString &channelId,
                                           const QString &deviceType,
                                           const QString &measurand,
                                           double scale,
                                           double azimuth,
                                           double x,
                                           double y,
                                           double z);
    Q_INVOKABLE void setSelectedChannelUnknown();
    Q_INVOKABLE void importMetadata(const QString &fileUrl);
    Q_INVOKABLE void exportMetadata(const QString &fileUrl);
    Q_INVOKABLE void importDataBody(const QString &fileUrl);
    Q_INVOKABLE void confirmImportDataBody(const QString &fileUrl);
    Q_INVOKABLE void exportDataBody(const QString &fileUrl);
    Q_INVOKABLE void exportHdf5Data(const QString &fileUrl);
    Q_INVOKABLE void loadExternalData(const QString &format,
                                      const QString &fileUrl,
                                      const QVariantMap &options = {});
    Q_INVOKABLE void cancelExternalImport();
    Q_INVOKABLE void clearExternalImport();
    Q_INVOKABLE void applyExternalImport(const QVariantList &targetChannels);
    Q_INVOKABLE void copySelectedCells();
    Q_INVOKABLE void selectAllData();
    Q_INVOKABLE void selectColumn(int col);
    Q_INVOKABLE void selectRow(int row);
    Q_INVOKABLE void updatePacketHeader(int sourceId,
                                        int sampleRate,
                                        int channelCount,
                                        int dataPointCount,
                                        int encoding,
                                        qlonglong timestamp);

signals:
    void headerUpdated();
    void metadataUpdated();
    void packetUpdated(); // 数据包更新信号
    void fileLoaded();    // 新增：文件整体加载完成信号
    void documentUpdated();
    void channelsUpdated();
    void selectedChannelUpdated();
    void geometryUpdated();
    void validationUpdated();
    void confirmDataImportNptsMismatch(const QString &fileUrl,
                                       int expectedNpts,
                                       int importedNpts,
                                       int importedChannels);
    void externalImportUpdated();
    void showMessage(const QString &message, bool isError = false);

private:
    void emitAllDocumentSignals();
    void refreshChannelModel();
    void setSelectedChannelRow(int row);
    void rebuildValidationReport(bool finalValidation = false);
    [[nodiscard]] QList<ValidationTableModel::Issue>
    collectValidationIssues(bool finalValidation = false) const;
    void importDataBodyInternal(const QString &fileUrl, bool acceptNptsChange);
#ifndef Q_MOC_RUN
    void handleExternalImportFinished();
    [[nodiscard]] qrest_data::Metadata metadataForExternalImport() const;
#endif

    DataTableModel *m_tableModel;          // 表格模型实例
    QItemSelectionModel *m_selectionModel; // 新增：选择模型实例
    ChannelTableModel *m_channelModel;
    QItemSelectionModel *m_channelSelectionModel;
    ValidationTableModel *m_validationModel;
    BinaryTableModel *m_binaryModel;
    int m_selectedChannelRow{-1};

    QrestDocument m_document;

#ifndef Q_MOC_RUN
    QFutureWatcher<ExternalImportResult> *m_externalImportWatcher{nullptr};
    qrest_data::tools::ExternalDataset m_externalDataset;
#endif
    bool m_externalImportLoading{false};
    bool m_externalImportReady{false};
    bool m_externalImportCanceled{false};
    QString m_externalImportFormat;
    QString m_externalImportPath;
    QString m_externalImportStatus;
};

#endif // QREST_VIEW_MODEL_H

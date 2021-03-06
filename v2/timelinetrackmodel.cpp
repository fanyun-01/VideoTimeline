#include "timelinetrackmodel.h" 
#include "mainwindow.h"    

#include <QScopedPointer>
#include <QApplication>
#include <qmath.h>
#include <QTimer> 
#include <QTime>

static const quintptr NO_PARENT_ID = quintptr(-1); 

namespace timeline {

	ClipInfo::ClipInfo(TrackIndex index) 
		: mTrackIndex(index),
		mIsBlank(false){

	}

	ClipInfo::~ClipInfo() {

	}

	TimelineTracksModel::TimelineTracksModel(QObject *parent)
		: QAbstractItemModel(parent),
		mScaleFactor(1.0),
		mTrackHeight(50) {
		load();
		connect(this, SIGNAL(modified()), SLOT(adjustBackgroundDuration()));
	}

	TimelineTracksModel::~TimelineTracksModel() {

	}

	int TimelineTracksModel::randNumber(int low, int high) {
		// Random number between low and high
		return qrand() % ((high + 1) - low) + low;
	}

	// one video track and one audio track
	int TimelineTracksModel::rowCount(const QModelIndex &parent) const {
		// get clips count per row
		if (parent.isValid()) {
			if (parent.internalId() != NO_PARENT_ID) {
				return 0;
			}  
			return mTracks[parent.row()].count();
		}
		// get tracks count
		return 2;
	}

	int TimelineTracksModel::columnCount(const QModelIndex &parent) const {
		Q_UNUSED(parent);
		return 1;
	}

	QVariant TimelineTracksModel::data(const QModelIndex &index, int role) const {
		if (!index.isValid())
		 	return QVariant();
		// Get info for a clip
		if (index.parent().isValid()) {
			TrackIndex trackIndex = (TrackIndex)index.internalId();
			int clipIndex = index.row();
			ClipInfo clipInfo = mTracks[trackIndex].at(clipIndex);
			// get clip info
			switch (role) {
			case NameRole:
				return clipInfo.getName(); 
			case SourceRole:
				return clipInfo.getSourcePath(); 
			case Qt::DisplayRole:
				return clipInfo.getName(); 
			case DurationRole:
				return clipInfo.getDuration(); 
			case InPointRole:
				return clipInfo.getInPoint(); 
			case OutPointRole:
				return clipInfo.getOutPoint();  
			case FrameRateRole:
				return clipInfo.getFrameRate();
			case AudioLevelsRole:
				return QVariant();
			case IsBlankRole:
				return clipInfo.isBlank();
			case IsAudioRole:
				return trackIndex == AudioTrack;
			default:
				break;
			} 
		}
		// Get info for a track
		else {
			TrackIndex trackIndex = (TrackIndex)index.row();
			switch (role) {
			case Qt::DisplayRole:
				return trackIndex == VideoTrack ? "Video Track" : "Audio Track"; 
			case DurationRole: { 
				return maxTrackLength();
			} 
			case IsAudioRole:
				return trackIndex == AudioTrack; 
			default:
				break;
			} 
		}
		return QVariant();
	}

	QModelIndex TimelineTracksModel::index(int row, int column, 
		const QModelIndex &parent) const {
		if (column > 0)
			return QModelIndex();

		QModelIndex result;
		if (parent.isValid()) { 
			// the internal identifier is track index
			result = createIndex(row, column, parent.row());
		}
		else if (row < tracksCount()) {
			result = createIndex(row, column, NO_PARENT_ID);
		}
		return result;
	}

	QModelIndex TimelineTracksModel::makeIndex(int trackIndex, int clipIndex) const {
		return index(clipIndex, 0, index(trackIndex));
	}

	QModelIndex TimelineTracksModel::parent(const QModelIndex &index) const { 
		if (!index.isValid() || index.internalId() == NO_PARENT_ID)
			return QModelIndex();
		else
			return createIndex(index.internalId(), 0, NO_PARENT_ID);
	}
	
	// exposed to QML
	QHash<int, QByteArray> TimelineTracksModel::roleNames() const {
		QHash<int, QByteArray> roles;
		roles[NameRole] = "name";
		roles[SourceRole] = "source";  
		roles[DurationRole] = "duration";
		roles[InPointRole] = "in";
		roles[OutPointRole] = "out"; 
		roles[IsAudioRole] = "audio";
		roles[IsBlankRole] = "blank";
		roles[FrameRateRole] = "fps";
		roles[AudioLevelsRole] = "audioLevels";   
		return roles;
	} 

	bool TimelineTracksModel::trimClipInValid(int trackIndex, int clipIndex, int delta) {
		return true;
	}

	int TimelineTracksModel::trimClipIn(int trackIndex, int clipIndex, int delta) { 
		return true;
	}

	void TimelineTracksModel::notifyClipIn(int trackIndex, int clipIndex) {
		if (trackIndex >= 0 && trackIndex < tracksCount() && clipIndex >= 0) {
			QModelIndex index = createIndex(clipIndex, 0, trackIndex);
			QVector<int> roles;
			roles << AudioLevelsRole;
			emit dataChanged(index, index, roles);
		} 
	}

	bool TimelineTracksModel::trimClipOutValid(int trackIndex, int clipIndex, int delta) {
		return true;
	}

	int TimelineTracksModel::trackHeight() const {
		return mTrackHeight;
	}

	void TimelineTracksModel::setTrackHeight(int height) { 
		mTrackHeight = height;
		emit trackHeightChanged();
	}

	double TimelineTracksModel::scaleFactor() const {
		return mScaleFactor;
	}

	void TimelineTracksModel::setScaleFactor(double scale) {  
		mScaleFactor = scale;
		emit scaleFactorChanged();
	}

	int TimelineTracksModel::trimClipOut(int trackIndex, int clipIndex, int delta) {
		return 0;
	}

	void TimelineTracksModel::notifyClipOut(int trackIndex, int clipIndex) {
		if (trackIndex >= 0 && trackIndex < tracksCount() && clipIndex >= 0) {
			QModelIndex index = createIndex(clipIndex, 0, trackIndex);
			QVector<int> roles;
			roles << AudioLevelsRole;
			emit dataChanged(index, index, roles); 
		} 
	}

	bool TimelineTracksModel::moveClipValid(int clipIndex, int position) {
		return true;
	}

	bool TimelineTracksModel::moveClip(int clipIndex, int position) { 
		return true;
	}  

	int TimelineTracksModel::appendClip(int trackIndex) {
		return -1;
	}

	void TimelineTracksModel::removeClip(int trackIndex, int clipIndex) {

	}

	void TimelineTracksModel::splitClip(int trackIndex, int clipIndex, int splitPosition) {
		ClipInfo oldClip;
		bool ret = getClipInfo(trackIndex, clipIndex, oldClip);
		if (ret) {
			int inPoint = oldClip.getInPoint();
			int outPoint = oldClip.getOutPoint();
			int duration = splitPosition - inPoint;

			// update old clip
			resizeClip(trackIndex, clipIndex, inPoint, inPoint + duration);
			QModelIndex modelIndex = createIndex(clipIndex, 0, trackIndex);
			QVector<int> updateRoles;
			updateRoles << DurationRole << OutPointRole;
			emit dataChanged(modelIndex, modelIndex, updateRoles);

			// insert new clip
			beginInsertRows(index(trackIndex), clipIndex + 1, clipIndex + 1);
			ClipInfo newClip((TrackIndex)trackIndex);
			newClip.setInPoint(inPoint + duration);
			newClip.setOutPoint(outPoint);
			newClip.setDuration(outPoint - inPoint - duration);
			newClip.setSourcePath(oldClip.getSourcePath());
			newClip.setName(oldClip.getName());
			insertClip(trackIndex, clipIndex + 1, newClip);
			endInsertRows();
		}
	}

	void TimelineTracksModel::joinClips(int trackIndex, int clipIndex) {
		
	}
	
	void TimelineTracksModel::moveClipToEnd(int trackIndex, int clipIndex, int position) {
		
	} 

	void TimelineTracksModel::audioLevelsReady(const QModelIndex& index) {
		QVector<int> roles;
		roles << AudioLevelsRole;
		emit dataChanged(index, index, roles);
	}

	void TimelineTracksModel::adjustBackgroundDuration() {
	
	}  

	void TimelineTracksModel::load() { 
		// load from XML file
		for (int j = 0; j < 2; ++j) { 
			ClipInfo info((TrackIndex)j);
			info.setInPoint(0);
			info.setDuration(randNumber(100, 300));
			info.setOutPoint(info.getInPoint() + info.getDuration());
			if (j == 0) {
				info.setName("VID_20180801.mp4");
			}
			else {
				info.setName("VID_20180801.mp3");
			}
			info.setSourcePath(QUrl("VID_20180801.mp4"));
			info.setFrameRate(30);
			info.setModelIndex(0);
			mTracks[j].push_back(info);
			
			ClipInfo info1((TrackIndex)j);
			info1.setInPoint(info.getOutPoint());
			info1.setDuration(randNumber(100, 300));
			info1.setOutPoint(info1.getInPoint() + info1.getDuration());
			info1.setFrameRate(30); 
			info1.setModelIndex(1);
			info1.setBlank(true);
			mTracks[j].push_back(info1);

			ClipInfo info2((TrackIndex)j);
			info2.setInPoint(info1.getOutPoint());
			info2.setDuration(randNumber(100, 300));
			info2.setOutPoint(info2.getInPoint() + info2.getDuration());
			if (j == 0) {
				info2.setName("VID_20180801.mp4");
			}
			else {
				info2.setName("VID_20180801.mp3");
			} 
			info2.setModelIndex(2);
			mTracks[j].push_back(info2);

			ClipInfo info3((TrackIndex)j);
			info3.setInPoint(info2.getOutPoint());
			info3.setDuration(randNumber(100, 300));
			info3.setOutPoint(info3.getInPoint() + info3.getDuration());
			if (j == 0) {
				info3.setName("VID_20180801.mp4");
			}
			else {
				info3.setName("VID_20180801.mp3");
			}
			info3.setModelIndex(3);
			mTracks[j].push_back(info3);
		}
	}

	void TimelineTracksModel::reload() { 
		beginResetModel();
		endResetModel();
		getAudioLevels();
	}

	void TimelineTracksModel::close() {
		emit closed();
	}

	int TimelineTracksModel::clipIndex(int trackIndex, int position) {
		return -1;
	}

	void TimelineTracksModel::getAudioLevels() {
		
	} 

	int TimelineTracksModel::maxTrackLength() const {
		int length = 0;
		for (size_t i = 0; i < 2; i++) {
			for (size_t j = 0; j < mTracks[i].count(); j++) {
				if (length < mTracks[i].at(j).getOutPoint()) {
					length = mTracks[i].at(j).getOutPoint();
				}
			}
		}
		return length;
	}  

	int TimelineTracksModel::clipsCount(int trackIndex) {
		if (trackIndex < 0 || trackIndex > tracksCount()) {
			return -1;
		}

		return mTracks[trackIndex].size();
	}

	bool TimelineTracksModel::getClipInfo(int trackIndex, int clipIndex, 
		ClipInfo& clipInfo) {
		if (mTracks[trackIndex].count() <= clipIndex) {
			return false;
		}
		clipInfo = mTracks[trackIndex][clipIndex];
		return true;
	}

	void TimelineTracksModel::resizeClip(int trackIndex, int clipIndex, 
		int inPoint, int outPoint) {
		if (trackIndex < 0 || trackIndex > tracksCount() || clipIndex < 0) {
			return;
		}
		ClipInfo& clip = mTracks[trackIndex][clipIndex];
		clip.setInPoint(inPoint);
		clip.setOutPoint(outPoint);
		clip.setDuration(outPoint - inPoint);
	}

	bool TimelineTracksModel::insertClip(int trackIndex, int clipIndex, const ClipInfo& clip) {
		if (trackIndex < 0 || trackIndex >= tracksCount() || clipIndex < 0) {
			return false;
		}
		mTracks[trackIndex].insert(clipIndex, clip);
		return true;
	}

	int TimelineTracksModel::getClipIndexAt(int trackIndex, int position) {
		if (trackIndex < 0 || trackIndex >= tracksCount() || position < 0) {
			return -1;
		}
		int clips = clipsCount(trackIndex);
		for (int clipIndex = 0; clipIndex < clips; ++clipIndex) {
			int inPos = mTracks[trackIndex][clipIndex].getInPoint();
			int outPos = mTracks[trackIndex][clipIndex].getOutPoint();
			if (inPos < position && outPos > position) {
				return clipIndex;
			}
		}
		
		return -1;
	}
}


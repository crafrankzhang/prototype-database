#include "rbfm.h"

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
    pf_manager = PagedFileManager::instance();
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) {
    return pf_manager->createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
    return pf_manager->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
    return pf_manager->openFile(fileName , fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    return pf_manager->closeFile(fileHandle);
}




RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid){

unsigned  int dataSize;
void * tempData = malloc(PAGE_SIZE);
changeDatatoFormatDataandCaucalateDataSize(recordDescriptor,data,dataSize ,tempData,rid);
insertRecordtoFile(fileHandle , dataSize, tempData,rid);
    return 0;
}


RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {

	void * temp_page = malloc(PAGE_SIZE);
	fileHandle.readPage(rid.pageNum -1 , temp_page);
	unsigned  int availableIndex;
	unsigned  int length;
	memcpy(&availableIndex, (byte*)temp_page + PAGE_SIZE - rid.slotNum * 2 * sizeof(unsigned  int) ,sizeof(unsigned  int));
	memcpy(&length, (byte*)temp_page + PAGE_SIZE - rid.slotNum * 2 * sizeof(unsigned  int) + sizeof(unsigned  int) ,sizeof(unsigned  int));
	memcpy(data, (byte*)temp_page+availableIndex+sizeof(rid.pageNum)+sizeof(rid.slotNum)+ recordDescriptor.size()*sizeof(unsigned  int),
		                     length-sizeof(rid.pageNum)-sizeof(rid.slotNum)-recordDescriptor.size()*sizeof(unsigned  int));
	free(temp_page);
	return 0;
}


RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
		unsigned fieldNumber = recordDescriptor.size();
		unsigned nullLength = ceil((double) fieldNumber / 8);
		unsigned char * nullFieldsIndicator = new unsigned char[nullLength];
		memcpy(nullFieldsIndicator, data, nullLength);
		int * fieldPosition = new int[fieldNumber];
		unsigned positionOffset = nullLength;
		for (unsigned i = 0; i < fieldNumber; i++) {

			cout << recordDescriptor[i].name << ":\t";

			bool nullBit = nullFieldsIndicator[i / 8]
					& (1 << ( 7 - i % 8));

			if (!nullBit) {
				AttrType type = recordDescriptor[i].type;
				if (type == TypeInt) {
					int * dataValue = new int;
					memcpy(dataValue, (byte*) data + positionOffset, sizeof(int));

					cout << * dataValue << "\t";

					delete dataValue;
					positionOffset += sizeof(int);
				} else if (type == TypeReal) {
					float * dataValue = new float();
					memcpy(dataValue, (byte*) data + positionOffset, sizeof(float));

					cout << * dataValue << "\t";

					delete dataValue;
					positionOffset += sizeof(float);
				} else {
					int length = 0;
					memcpy(&length, (byte*) data + positionOffset, sizeof(int));
					positionOffset += sizeof(int);

					char * dataValue = new char[length + 1];
					memcpy(dataValue, (byte*) data + positionOffset,
							length * sizeof(char));
					dataValue[length] = '\0';

					cout << (char*) (dataValue) << "\t\t";

					delete[] dataValue;
					positionOffset += length;
				}
			} else {
				cout << "NULL" << "\t";
			}
			fieldPosition[i] = positionOffset;
		}
		cout << "" << endl;
		delete[] nullFieldsIndicator;
		return 0;
}


RC RecordBasedFileManager::changeDatatoFormatDataandCaucalateDataSize(const vector<Attribute> &recordDescriptor, const void *data,unsigned  int & dataSize ,
		void * tempData, RID &rid){

	unsigned int fieldNum = recordDescriptor.size();
	unsigned int nullFieldLength = ceil((double)fieldNum / 8);
	unsigned char *nullsIndicator = (unsigned char *) malloc(nullFieldLength);
    memcpy(nullsIndicator ,data,nullFieldLength);
	unsigned  int * fieldPosition = new unsigned  int [fieldNum];
	unsigned short int offsetForField = 0;
	unsigned  int offset=0;
	rid.pageNum = 0;
	rid.slotNum = 0;
	memcpy((byte*)tempData+offset ,&rid.pageNum,sizeof(rid.pageNum) );
	offset+=sizeof(rid.pageNum);
	memcpy((byte*)tempData+offset, &rid.slotNum,sizeof(rid.slotNum));
	offset+=sizeof(rid.slotNum);
	memcpy((byte*)tempData+offset+recordDescriptor.size() * sizeof(unsigned  int),data,nullFieldLength);
	offsetForField=offset;
	offset += recordDescriptor.size() * sizeof(unsigned  int)+nullFieldLength;
	unsigned int dataOffset =nullFieldLength;
	for(int i =0; i < fieldNum;i++){
	     fieldPosition[i] = offset;
		 memcpy((byte*)tempData+offsetForField,&fieldPosition[i],sizeof(unsigned  int));
		 bool nullBit = nullsIndicator[i / 8]
						& (1 << (8 - 1 - i % 8));
				if (!nullBit) { // not null
					AttrType type = recordDescriptor[i].type;
					if (type == TypeInt) {
						memcpy((byte*)tempData+offset ,(byte*)data+dataOffset ,sizeof(int));

						offset += sizeof(int);
						dataOffset += sizeof(int);

					} else if (type == TypeReal) {

						memcpy((byte*)tempData+offset ,(byte*)data+dataOffset ,sizeof(float));
				        dataOffset += sizeof(float);
						offset +=sizeof(float);
					} else {
						int length = 0;
						memcpy(&length, (byte*) data + dataOffset, sizeof(int));
						memcpy((byte*)tempData+offset,(byte*)data+dataOffset, sizeof(int)+length);
						offset +=sizeof(int)+length;
						dataOffset +=sizeof(int)+length;
					}
				} else {

				}
	              offsetForField += sizeof(unsigned  int );
		}
	dataSize = offset;
	free(nullsIndicator);
    return 0;
}


RC RecordBasedFileManager::insertRecordtoFile(FileHandle &fileHandle,unsigned  int &dataSize , void* tempData,RID &rid){
	unsigned int totalPageNum = fileHandle.getNumberOfPages();
	unsigned  int availableIndex ;
	bool needAppend = true;
	unsigned int pageIndex = 1;
	unsigned  int slotNumber ;
	if(totalPageNum < 1){
		unsigned  int init = 0;
		availableIndex = dataSize;
		slotNumber = 2;
		memcpy((byte*) tempData + PAGE_SIZE - 2 * sizeof(unsigned  int),
					&availableIndex, sizeof(unsigned  int));

		memcpy((byte*) tempData + PAGE_SIZE - sizeof(unsigned  int),
					&slotNumber, sizeof(unsigned  int));

		memcpy((byte*) tempData + PAGE_SIZE - 4 * sizeof(unsigned  int),
						&init, sizeof(unsigned  int));

		memcpy((byte*) tempData + PAGE_SIZE - 3 * sizeof(unsigned  int),
						&dataSize, sizeof(unsigned  int));

		memcpy((byte*) tempData ,
							&rid.pageNum, sizeof(rid.pageNum));
		memcpy((byte*) tempData + sizeof(rid.pageNum),
							&rid.slotNum, sizeof(rid.slotNum));
		rid.pageNum = 1;
		rid.slotNum = 2;
		fileHandle.appendPage(tempData);
		needAppend = false;

	}else{

		void * tempPageData = malloc(PAGE_SIZE);

			while(pageIndex <=  totalPageNum){

				fileHandle.readPage(pageIndex -1, tempPageData);

				memcpy(&availableIndex,
							(byte*) tempPageData + PAGE_SIZE - 2 * sizeof(unsigned  int),
							sizeof(unsigned  int));
				memcpy(&slotNumber,(byte*) tempPageData + PAGE_SIZE - sizeof(unsigned  int),
							sizeof(unsigned  int));

				if (availableIndex * sizeof(byte)
							+ (slotNumber + 1) * 2 * sizeof(unsigned  int)
							+ dataSize < PAGE_SIZE) {
							needAppend = false;
					} else {

					}
				if(!needAppend){

					memcpy(&slotNumber,(byte*) tempPageData + PAGE_SIZE - sizeof(unsigned  int),
											sizeof(unsigned  int));
					slotNumber++;
					memcpy((byte*)tempPageData + PAGE_SIZE - sizeof(unsigned  int),&slotNumber,
											sizeof(unsigned  int));
					//memcpy((byte *)tempData, &pageIndex , sizeof(pageIndex));
					//memcpy((byte*)tempData+sizeof(pageIndex),&slotNumber,sizeof(slotNumber));
					memcpy((byte*)tempPageData + PAGE_SIZE - slotNumber * 2 * sizeof(unsigned  int),&availableIndex ,
							sizeof(unsigned  int));
					memcpy((byte*)tempPageData + PAGE_SIZE - slotNumber * 2 * sizeof(unsigned int) + sizeof(unsigned  int),&dataSize ,
							sizeof(unsigned  int));
					memcpy((byte*)tempPageData+availableIndex ,tempData,dataSize);
					memcpy(&availableIndex,(byte*) tempPageData + PAGE_SIZE - 2 * sizeof(unsigned  int),
															sizeof(unsigned  int));
					availableIndex +=dataSize;
					memcpy((byte*) tempPageData + PAGE_SIZE - 2* sizeof(unsigned  int),&availableIndex,
															sizeof(unsigned  int));
					fileHandle.writePage(pageIndex-1,tempPageData);
					free(tempPageData);
					  rid.pageNum = pageIndex;
					  rid.slotNum =slotNumber;
					break;
				}
				pageIndex++;
			}
	}
	if(needAppend){

			unsigned  int init = 0;
			rid.pageNum = pageIndex ;
			rid.slotNum = 2;
			availableIndex = dataSize;
			slotNumber = 2;
			memcpy((byte*) tempData + PAGE_SIZE - 2 * sizeof(unsigned  int),
						&availableIndex, sizeof(unsigned  int));
			memcpy((byte*) tempData + PAGE_SIZE - sizeof(unsigned  int),
						&slotNumber, sizeof(unsigned  int));
			memcpy((byte*) tempData + PAGE_SIZE - 4 * sizeof(unsigned  int),
							&init, sizeof(unsigned  int));
			memcpy((byte*) tempData + PAGE_SIZE - 3*sizeof(unsigned  int),
							&dataSize, sizeof(unsigned  int));
			memcpy((byte*) tempData ,
								&rid.pageNum, sizeof(rid.pageNum));
			memcpy((byte*) tempData + sizeof(rid.pageNum),
								&rid.slotNum, sizeof(rid.slotNum));
			fileHandle.appendPage(tempData);
			free(tempData);
	}
return 0;
}


RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid){

	if (deleteFormatDataFromFile(fileHandle, rid) != 0){

		cout<<"delete record fail"<<endl;
		return -1;
	}

	return 0;

}

RC RecordBasedFileManager::deleteFormatDataFromFile(FileHandle &fileHandle,const RID &rid){

	void * tempPage = malloc(PAGE_SIZE);
	fileHandle.readPage(rid.pageNum , tempPage);

	void * tempFormatData = malloc(PAGE_SIZE);
	unsigned int dataSize;
	if(getFormatDataFromPage(tempPage, tempFormatData , dataSize, rid.slotNum) != 0){
		return -1;
	}
    bool isTombStone = false;
    isTombStoneRecord(tempFormatData ,isTombStone);
    if(isTombStone){
    	RID tempRid;
    	memcpy(&tempRid.pageNum, (byte*) tempFormatData, sizeof(unsigned));
    	memcpy(&tempRid.slotNum, (byte*) tempFormatData+sizeof(unsigned), sizeof(unsigned));

    	deleteFormatDataFromFile(fileHandle,tempRid);
    }
    free(tempFormatData );

    if(deleteRecordFromPage(tempPage, rid.slotNum) != 0){

    	return -1;
    }
    fileHandle.writePage(rid.pageNum, tempPage);
    free(tempPage);
    return 0;


}


RC RecordBasedFileManager:: getFormatDataFromPage(const void * tempPage, void * tempFormatData ,unsigned int & dataSize, unsigned slotNum){

	unsigned int slotStartPosition;
	unsigned int slotOffset;
	unsigned int offset = PAGE_SIZE-slotNum * 2* sizeof(unsigned int);
	memcpy(&slotStartPosition, (byte*) tempPage + offset,
				sizeof(unsigned int));
	memcpy(&slotOffset,
				(byte*) tempPage + offset + sizeof(unsigned int),
				sizeof(unsigned int));

	if(slotOffset == 0){
		cout<<"the record has already been deleted"<<endl;
		return -1;
	}

	memcpy(tempFormatData, (byte*)tempPage + slotStartPosition, slotOffset);
	dataSize = slotOffset;
	return 0;
}

RC RecordBasedFileManager::isTombStoneRecord(const void * tempFormatData , bool& isTombstone ){
	isTombstone = false;
	unsigned pageNumber;
	memcpy(&pageNumber, (byte*) tempFormatData, sizeof(pageNumber));
	if(pageNumber !=0){
		isTombstone = true;
	}
return 0;
}

RC  RecordBasedFileManager::deleteRecordFromPage(void * tempPage , unsigned slotNum){
	unsigned  int slotStartPosition;
	unsigned  int slotOffset;
	unsigned  int offset = PAGE_SIZE - 2 * sizeof(unsigned  int) * (slotNum);
		memcpy(&slotStartPosition, (byte*)  tempPage+ offset,
				sizeof(unsigned int));
		memcpy(&slotOffset,
				(byte*) tempPage+ offset + sizeof(unsigned int),
				sizeof(unsigned int));
		if(slotOffset == 0){
			return -1;
		}
		unsigned int numRecord;
		unsigned int availableSlotIndex;
		offset = PAGE_SIZE - 2*sizeof(unsigned);
		memcpy(&availableSlotIndex,(byte*) tempPage+ offset,sizeof(unsigned int));
		memcpy(&numRecord,(byte*) tempPage+ offset+sizeof(unsigned int),sizeof(unsigned int));
		if (availableSlotIndex > slotStartPosition + slotOffset) {
				memmove((byte*) tempPage + slotStartPosition ,
						(byte*) tempPage+ slotStartPosition + slotOffset,
						availableSlotIndex - slotStartPosition
								- slotOffset);
			}
		updateRecordDirInfo(tempPage, slotNum, slotStartPosition, 0);

		unsigned tempNumRecord = numRecord;
		while(tempNumRecord>1){

			unsigned  int currentSlotStartPosition;
			unsigned  int currentSlotOffset;
			unsigned  int currentOffset = PAGE_SIZE - 2 * sizeof(unsigned  int) * (tempNumRecord);

			memcpy(&currentSlotStartPosition, (byte*)  tempPage+ offset,
							sizeof(unsigned int));
			memcpy(&currentSlotOffset,
							(byte*) tempPage+ offset + sizeof(unsigned int),
							sizeof(unsigned int));
			if(currentSlotStartPosition >slotStartPosition){
				currentSlotStartPosition -= slotOffset;
			}
			updateRecordDirInfo(tempPage,tempNumRecord,currentSlotStartPosition,currentSlotOffset);
			tempNumRecord--;

		}

		availableSlotIndex -= slotOffset;
		updatePageInfo(tempPage,availableSlotIndex,numRecord);

return 0;



}
RC RecordBasedFileManager::updateRecordDirInfo(void * tempPage , unsigned slotNum, unsigned  slotStartPosition, unsigned  int slotOffset ){

	unsigned offset = PAGE_SIZE-2*(sizeof(unsigned))*(slotNum);
	memcpy((byte*) tempPage+ offset,&slotStartPosition,sizeof(unsigned int));
	memcpy((byte*) tempPage+ offset+sizeof(unsigned),&slotOffset,sizeof(unsigned int));

  return 0;


}
RC RecordBasedFileManager::updatePageInfo(void * tempPage,unsigned  availablePosition, unsigned numRecord ){

	   	unsigned offset = PAGE_SIZE-2*(sizeof(unsigned));
		memcpy((byte*) tempPage+ offset,&availablePosition,sizeof(unsigned int));
		memcpy((byte*) tempPage+ offset+sizeof(unsigned),&numRecord,sizeof(unsigned int));
		return 0;
}


RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle,
		const vector<Attribute> &recordDescriptor, const void *data,
		const RID &rid) {
    RID tempRid;
    tempRid.pageNum = rid.pageNum;
    tempRid.slotNum = rid.slotNum;
	void * tempFormatData = malloc(PAGE_SIZE);
	unsigned  int dataSize;
	changeDatatoFormatDataandCaucalateDataSize(recordDescriptor, data,dataSize ,tempFormatData,tempRid);
	updateFormatDataFromFile(fileHandle, tempFormatData, dataSize, tempRid);

	return 0;
}

RC RecordBasedFileManager::updateFormatDataFromFile(FileHandle &fileHandle,void * tempData, unsigned int dataSize,RID & tempRid ){

	void * tempPage = malloc(PAGE_SIZE);
	fileHandle.readPage(tempRid.pageNum, tempPage);
	bool availableUpdateCurrentPage = false;


	{
			// delete linked record if current record is a link
			unsigned  int oldTempFormatDataSize;
			void * oldTempFormatData = malloc(PAGE_SIZE);
			getFormatDataFromPage(tempPage, oldTempFormatData,oldTempFormatDataSize, tempRid.slotNum);

			bool isTombStone = false;
			 isTombStoneRecord(oldTempFormatData ,  isTombStone );


			if (isTombStone) {

				RID firstTempRid;
				memcpy(&firstTempRid.pageNum,(byte*)oldTempFormatData ,sizeof(unsigned int));
				memcpy(&firstTempRid.slotNum,(byte*)oldTempFormatData+sizeof(unsigned),sizeof(unsigned int));
				deleteFormatDataFromFile(fileHandle, firstTempRid);

			}
			isAvailableUpdateCurrentPage(tempPage, dataSize,oldTempFormatDataSize, availableUpdateCurrentPage) ;
			free(oldTempFormatData);
		}
       if(availableUpdateCurrentPage){
    	   updateRecordFromPage(tempPage, tempData, dataSize,tempRid.slotNum);

    	   	fileHandle.writePage(tempRid.pageNum, tempPage);
    	   		return 0;
       }
       RID newTempRid;
       insertRecordtoFile(fileHandle,dataSize , tempData,newTempRid);
       dataSize = sizeof(RID);
       memcpy((byte*)tempData,&newTempRid.pageNum,sizeof(unsigned int));
       memcpy((byte*)tempData+sizeof(unsigned),&newTempRid.slotNum,sizeof(unsigned int));
       updateRecordFromPage(tempPage, tempData, dataSize,tempRid.slotNum);
       fileHandle.writePage(tempRid.pageNum, tempPage);
       return 0;

}


RC RecordBasedFileManager::isAvailableUpdateCurrentPage(void * tempPage, unsigned int dataSize,unsigned int oldTempFormatDataSize,bool &  availableUpdateCurrentPage){

	unsigned int dataDistance = dataSize -oldTempFormatDataSize;

		if (dataDistance <= 0) {
			availableUpdateCurrentPage = true;
			return 0;
		}

		unsigned short int availableSlotPosition;
		unsigned short int numRecord;
		memcpy(&availableSlotPosition,(byte*)tempPage+PAGE_SIZE-2 * sizeof(unsigned int) ,sizeof(unsigned int));
		memcpy(&numRecord,(byte*)tempPage+PAGE_SIZE- sizeof(unsigned int) ,sizeof(unsigned int));
		if (availableSlotPosition +dataDistance
				+ (numRecord) * 2 * sizeof(unsigned int) < PAGE_SIZE) {

			availableUpdateCurrentPage = true;
		} else {
			availableUpdateCurrentPage = false;
		}
		return 0;
}

RC RecordBasedFileManager::updateRecordFromPage(void * tempPage, void * tempData, unsigned dataSize , unsigned slotNum){

	unsigned int availableSlotPosition;
	unsigned int numRecord;
	memcpy(&availableSlotPosition,(byte*)tempPage+PAGE_SIZE-2 * sizeof(unsigned int) ,sizeof(unsigned int));
	memcpy(&numRecord,(byte*)tempPage+PAGE_SIZE- sizeof(unsigned int) ,sizeof(unsigned int));
	unsigned int currentSlotStartPosition;
	unsigned int currentSlotOffset;
	memcpy(&currentSlotStartPosition,(byte*)tempPage+PAGE_SIZE-2 * sizeof(unsigned int)*slotNum ,sizeof(unsigned int));
	memcpy(&currentSlotOffset,(byte*)tempPage+PAGE_SIZE- 2 * sizeof(unsigned int)*slotNum+sizeof(unsigned int) ,sizeof(unsigned int));


	//	if((dataSize - currentSlotOffset + availableSlotPosition + numRecord * 2 * sizeof(unsigned short int)) >= PAGE_SIZE){
	//		cout << "!!!calculate error"<< endl;
	//		return -1;
	//	}

		memmove((byte*) tempPage + currentSlotStartPosition + dataSize,
				(byte*) tempPage + currentSlotStartPosition + currentSlotOffset,
				availableSlotPosition -currentSlotStartPosition - currentSlotOffset);
		unsigned int moveOffset = dataSize - currentSlotOffset;
		memcpy((byte*) tempPage + currentSlotStartPosition, tempData, dataSize);
		currentSlotOffset = dataSize;
		updateRecordDirInfo(tempPage , slotNum, currentSlotStartPosition, currentSlotOffset);

		unsigned  int tempNumRecord = numRecord;
			while ( tempNumRecord > 1) {
				unsigned int tempCurrentSlotPosition;
				unsigned int tempCurrentSlotOffset;
				memcpy(&tempCurrentSlotPosition,(byte*)tempPage+PAGE_SIZE-2 * sizeof(unsigned int)*tempNumRecord ,sizeof(unsigned int));
				memcpy(&tempCurrentSlotOffset,(byte*)tempPage+PAGE_SIZE- 2 * sizeof(unsigned int)*tempNumRecord+sizeof(unsigned int) ,sizeof(unsigned int));
				tempCurrentSlotOffset += moveOffset;
				updateRecordDirInfo(tempPage , tempNumRecord, tempCurrentSlotPosition, tempCurrentSlotOffset );
				tempNumRecord --;
			}
			availableSlotPosition += moveOffset;
			updatePageInfo(tempPage,  availableSlotPosition, numRecord );
			return 0;
}
















#include "qe.h"
bool Iterator::CompareV(void * res, Value right, CompOp compOp, int isnull)
{
	AttrType type = right.type;
	void * rvalue = malloc(PAGE_SIZE);
	if (type == TypeVarChar)
	{
		int len = 0;
		memcpy(&len, (char*)right.data, sizeof(int));
		memcpy(rvalue, (char*)right.data, sizeof(int) + len);

	}
	else
	{
		memcpy(rvalue, (char*)right.data, sizeof(int));
	}
	if (compOp == NO_OP) return true;
	else if (isnull == 1) return false;

	bool result = false;
	if (type == TypeInt)
	{
		int condition = *((int *)rvalue);
		int record;
		memcpy(&record, (char*)res, sizeof(int));

		if ((compOp == EQ_OP && condition == record) ||
			(compOp == LT_OP && condition > record) ||
			(compOp == LE_OP && condition >= record) ||
			(compOp == GT_OP && condition < record) ||
			(compOp == GE_OP && condition <= record) ||
			(compOp == NE_OP && condition != record)
			)
		{
			result = true;
		}

	}
	else if (type == TypeReal)
	{

		int condition = *((float *)rvalue);
		float record = 0;
		memcpy(&record, (char*)res, sizeof(float));

		if ((compOp == EQ_OP && condition == record) ||
			(compOp == LT_OP && condition > record) ||
			(compOp == LE_OP && condition >= record) ||
			(compOp == GT_OP && condition < record) ||
			(compOp == GE_OP && condition <= record) ||
			(compOp == NE_OP && condition != record)
			)
		{
			result = true;
		}

	}
	else
	{
		string condition;
		int varCharLength;
		memcpy(&varCharLength, (char*)rvalue, sizeof(int));

		for (int i = 0; i < varCharLength; i++)
		{
			condition += *((char *)rvalue + 4 + i);
		}



		int varlen = 0;
		memcpy(&varlen, (char*)res, sizeof(int));
		char* record = new char[varlen + 1];
		memcpy(record, (char*)res + sizeof(int), varlen);
		record[varlen] = '\0';




		if ((compOp == EQ_OP && condition == record) ||
			(compOp == LT_OP && condition > record) ||
			(compOp == LE_OP && condition >= record) ||
			(compOp == GT_OP && condition < record) ||
			(compOp == GE_OP && condition <= record) ||
			(compOp == NE_OP && condition != record))
		{

			result = true;
		}


	}
	free(rvalue);
	return result;

}
RC Iterator::getRattr(vector<Attribute> attrs, void * data, string required, void * res, AttrType &type, int &isnull)
{
	RC rc = -1;
	unsigned int nullFieldLength = ceil((double)attrs.size() / 8);

	unsigned char *nullsIndicator = (unsigned char *)malloc(nullFieldLength);
	unsigned int dataOffset = nullFieldLength;
	memcpy(nullsIndicator, data, nullFieldLength);

	for (int i = 0; i< attrs.size(); i++)
	{
		Attribute attr = attrs[i];
		bool nullBit = nullsIndicator[i / 8]
			& (1 << (8 - 1 - i % 8));
		if (!nullBit) { // not null
			type = attr.type;


			if (required == attr.name)
			{
				if (type == TypeInt) {
					memcpy((byte*)res, (byte*)data + dataOffset, sizeof(int));

				}
				else if (type == TypeReal) {

					memcpy((byte*)res, (byte*)data + dataOffset, sizeof(float));

				}
				else {
					int length = 0;
					memcpy(&length, (byte*)data + dataOffset, sizeof(int));
					memcpy((byte*)res, (byte*)data + dataOffset, sizeof(int) + length);

				}
				rc = 0;
				break;
			}
			if (type == TypeInt) {



				dataOffset += sizeof(int);

			}
			else if (type == TypeReal) {


				dataOffset += sizeof(float);

			}
			else {
				int length = 0;
				memcpy(&length, (byte*)data + dataOffset, sizeof(int));

				dataOffset += sizeof(int) + length;

			}


		}
		else
		{
			type = attr.type;
			if (required == attr.name)
			{
				res = NULL;
				isnull = 1;
				rc = 0;
				break;
			}

		}

	}
	free(nullsIndicator);

	return rc;
}

Filter::Filter(Iterator* input, const Condition &condition) {
	this->input = input;
	this->condition = condition;
	input->getAttributes(attrs);
}
bool Filter::issatisfed(void * data)
{
	bool satisfed = false;
	void *res = malloc(PAGE_SIZE);
	AttrType ltype;
	int isnull = 0;
	RC rc = getRattr(attrs, data, condition.lhsAttr, res, ltype, isnull);
	if (rc == 0)
	{
		if (!condition.bRhsIsAttr)
		{
			if (condition.rhsValue.type == ltype)
			{
				satisfed = CompareV(res, condition.rhsValue, condition.op, isnull);
			}
		}
		else
		{

			return false;
		}


	}
	free(res);
	return satisfed;
}

// ... the rest of your implementations go here
RC Filter::getNextTuple(void *data)
{
	bool satisfed = false;
	RC rc = -1;
	while (!satisfed)
	{

		rc = input->getNextTuple(data);
		if (rc == -1) {
			break;
		}
		satisfed = issatisfed(data);
	}

	return rc;

}

Project::Project(Iterator *input, const vector<string> &attrNames) {
	this->input = input;
	attrname = attrNames;
	input->getAttributes(allattrs);
	for (int i = 0; i<allattrs.size(); i++)
		for (int j = 0; j < attrNames.size(); j++)
		{
			if (allattrs[i].name == attrNames[j])
			{
				required.push_back(allattrs[i]);

			}

		}

}


RC Project::getNextTuple(void *data) {
	RC rc = 0;
	rc = input->getNextTuple(data);
	if (rc != -1) {
		void * ndata = malloc(PAGE_SIZE);
		unsigned int nullFieldLength = ceil((double)allattrs.size() / 8);

		unsigned char *nullsIndicator = (unsigned char *)malloc(nullFieldLength);
		unsigned int dataOffset = nullFieldLength;
		memcpy(nullsIndicator, data, nullFieldLength);

		unsigned int nullFieldLengthnew = ceil((double)required.size() / 8);

		unsigned char *nullsIndicatornew = (unsigned char *)malloc(nullFieldLengthnew);
		unsigned int dataOffsetnew = nullFieldLengthnew;
		memset(nullsIndicatornew, 0, nullFieldLengthnew);
		for (int i = 0; i < allattrs.size(); i++)
		{



			Attribute attr = allattrs[i];
			bool isrequired = false;
			for (int j = 0; j < attrname.size(); j++)
			{
				if (attr.name == attrname[j])
				{
					isrequired = true;
					break;
				}
			}

			bool nullBit = nullsIndicator[i / 8]
				& (1 << (8 - 1 - i % 8));
			if (!nullBit) { // not null
				AttrType type = attr.type;


				if (isrequired)
				{

					if (type == TypeInt) {
						memcpy((byte*)ndata + dataOffsetnew, (byte*)data + dataOffset, sizeof(int));
						dataOffsetnew += sizeof(int);



					}
					else if (type == TypeReal) {

						memcpy((byte*)ndata + dataOffsetnew, (byte*)data + dataOffset, sizeof(float));
						dataOffsetnew += sizeof(int);


					}
					else {
						int length = 0;
						memcpy(&length, (byte*)data + dataOffset, sizeof(int));
						memcpy((byte*)ndata + dataOffsetnew, (byte*)data + dataOffset, sizeof(int) + length);
						dataOffsetnew += sizeof(int) + length;
					}

				}
				if (type == TypeInt) {
					dataOffset += sizeof(int);
				}
				else if (type == TypeReal) {
					dataOffset += sizeof(float);
				}
				else {
					int length = 0;
					memcpy(&length, (byte*)data + dataOffset, sizeof(int));
					dataOffset += sizeof(int) + length;

				}


			}
			else
			{

				if (isrequired)
				{

					nullsIndicatornew[i / 8] = nullsIndicatornew[i / 8] | (1 << (7 - i % 8));
				}

			}



		}

		memcpy(ndata, nullsIndicatornew, nullFieldLengthnew);
		memcpy(data, ndata, dataOffsetnew);

		free(nullsIndicator);
		free(ndata);
		free(nullsIndicatornew);



	}

	return rc;
}

BNLJoin::BNLJoin(Iterator *leftIn, TableScan *rightIn, const Condition &condition, const unsigned numPages)
{
	this->leftIn = leftIn;
	this->rightIn = rightIn;
	this->condition = condition;
	rdata = malloc(ONE_INTERNAL_SIZE);
	leftIn->getAttributes(lattr);
	np = numPages;
	rightIn->getAttributes(rattr);
	joinattr.clear();
	for (int i = 0; i < lattr.size(); i++)
		joinattr.push_back(lattr[i]);
	for (int i = 0; i < rattr.size(); i++)
		joinattr.push_back(rattr[i]);
	curpos = 0;
	firsttime = true;

}
bool BNLJoin::getleft(void * ldata)//find valid in block
{
	string strres;
	float floatres;
	int intres;
	getV(0, NULL, strres, intres, floatres);//get
	AttrType type = condition.rhsValue.type;

	if (type == TypeVarChar)
	{
		if (stringmap.count(strres) != 0)
		{

			vector<void *> tmp = stringmap[strres];
			memcpy(ldata, tmp[curpos++], ONE_INTERNAL_SIZE);
			if (curpos == tmp.size()) curpos = 0;
			return true;
		}
		else
		{
			return false;
		}

	}
	else if (type == TypeInt)
	{
		if (intmap.count(intres) != 0)
		{

			vector<void *> tmp = intmap[intres];
			memcpy(ldata, tmp[curpos++], ONE_INTERNAL_SIZE);
			if (curpos == tmp.size()) curpos = 0;
			return true;
		}
		else
		{
			return false;
		}

	}
	else
	{

		if (floatmap.count(floatres) != 0)
		{

			vector<void *> tmp = floatmap[floatres];
			memcpy(ldata, tmp[curpos++], ONE_INTERNAL_SIZE);
			if (curpos == tmp.size()) curpos = 0;
			return true;
		}
		else
		{
			return false;
		}
	}



}
void BNLJoin::getV(int type, void *ldata, string & strres, int & intres, float & floatres)
{
	void *data = malloc(ONE_INTERNAL_SIZE);
	int isnull = 0;
	if (type == 0) getRattr(rattr, rdata, condition.rhsAttr, data, condition.rhsValue.type, isnull);
	else getRattr(lattr, ldata, condition.lhsAttr, data, condition.rhsValue.type, isnull);
	if (condition.rhsValue.type == TypeVarChar)
	{
		int len;
		memcpy(&len, data, sizeof(int));
		memcpy(&strres, (char*)data + sizeof(int), len);


	}
	else if (condition.rhsValue.type == TypeInt)
	{
		memcpy(&intres, (char*)data, sizeof(int));

	}
	else
	{

		memcpy(&floatres, (char*)data, sizeof(int));
	}

}
RC BNLJoin::getNextTuple(void *data)
{
	void *ldata = malloc(ONE_INTERNAL_SIZE);
	RC rc;
	bool flag = false;

	if (curpos != 0)
	{
		getleft(ldata);
		flag = true;
	}
	else
	{

		while (getright())
		{
			if (getleft(ldata))
			{
				flag = true;
				break;

			}
		}


	}



	if (flag)
		jointwopart(ldata, rdata, data);
	free(ldata);
	if (flag)
		return 0;
	return -1;
}

bool BNLJoin::getright()
{
	RC rc = -1;
	while (rc != 0)
	{
		rc = rightIn->getNextTuple(rdata);
		if (rc != 0 || firsttime)
		{
			rc = updateleft();

			if (rc != 0)
				return false;
			else
				if (!firsttime)
					rightIn->setIterator();

			firsttime = false;
		}

	}
	return true;
}
//left in mem
RC BNLJoin::updateleft()
{
	int i;
	for (i = 0; i < lattr.size(); i++)
		if (condition.lhsAttr == lattr[i].name)
			break;

	AttrType tp = lattr[i].type;
	if (tp == TypeVarChar)
	{
		unordered_map<string, vector<void *>>::iterator iterstr;
		for (iterstr = stringmap.begin(); iterstr != stringmap.end(); iterstr++) {
			vector<void *>::iterator iterVector;
			for (iterVector = (*iterstr).second.begin(); iterVector != (*iterstr).second.end(); iterVector++) {
				free(*iterVector);
			}
		}
		stringmap.clear();
	}
	else if (tp == TypeReal)
	{

		unordered_map<float, vector<void *>>::iterator iterstr;
		for (iterstr = floatmap.begin(); iterstr != floatmap.end(); iterstr++) {
			vector<void *>::iterator iterVector;
			for (iterVector = (*iterstr).second.begin(); iterVector != (*iterstr).second.end(); iterVector++) {
				free(*iterVector);
			}
		}
		floatmap.clear();
	}
	else
	{
		unordered_map<int, vector<void *>>::iterator iterstr;
		for (iterstr = intmap.begin(); iterstr != intmap.end(); iterstr++) {
			vector<void *>::iterator iterVector;
			for (iterVector = (*iterstr).second.begin(); iterVector != (*iterstr).second.end(); iterVector++) {
				free(*iterVector);
			}
		}
		intmap.clear();
	}
	string str;
	int intv;
	float floatv;
	for (i = 0; i <(int)np * 200; i++)
	{
		void * data = malloc(ONE_INTERNAL_SIZE);
		RC rc = leftIn->getNextTuple(data);
		if (rc == 0)
		{
			if (tp == TypeInt)
			{
				getV(1, data, str, intv, floatv);
				intmap[intv].push_back(data);
			}
			else if (tp == TypeReal)
			{
				getV(1, data, str, intv, floatv);
				floatmap[floatv].push_back(data);

			}
			else
			{
				getV(1, data, str, intv, floatv);
				stringmap[str].push_back(data);

			}
		}
		else
		{
			if (i == 0) {

				rc = -1;
				free(data);
				return -1;

			}
			else {

				rc = 0;
				free(data);
				break;
			}

		}

	}

	return 0;
}

void BNLJoin::jointwopart(void * ldata, void * rdata, void * data)
{

	unsigned int lnullFieldLength = ceil((double)lattr.size() / 8);

	unsigned char *lnullsIndicator = (unsigned char *)malloc(lnullFieldLength);


	unsigned int rnullFieldLength = ceil((double)rattr.size() / 8);

	unsigned char *rnullsIndicator = (unsigned char *)malloc(rnullFieldLength);

	unsigned int nownullFieldLength = ceil((double)(rattr.size() + lattr.size()) / 8);

	memcpy(lnullsIndicator, ldata, lnullFieldLength);
	memcpy(rnullsIndicator, rdata, rnullFieldLength);
	memcpy((char*)data, ldata, lattr.size());
	int offset = lattr.size();
	memcpy((char*)data + offset, rdata, rattr.size());

	int llen = lnullFieldLength;
	for (int i = 0; i < lattr.size(); i++)
	{
		if ((lnullsIndicator[i / 8] & (1 << (7 - i % 8))) == 0) {
			if (lattr[i].type == TypeVarChar)
			{
				int len = 0;
				memcpy(&len, (char*)ldata + llen, sizeof(int));
				llen += sizeof(int) + len;

			}
			else
			{
				llen += sizeof(int);
			}
		}
	}
	llen -= lnullFieldLength;

	int rlen = rnullFieldLength;
	for (int i = 0; i < rattr.size(); i++)
	{
		if ((rnullsIndicator[i / 8] & (1 << (7 - i % 8))) == 0) {
			if (rattr[i].type == TypeVarChar)
			{
				int len = 0;
				memcpy(&len, (char*)rdata + rlen, sizeof(int));
				rlen += sizeof(int) + len;

			}
			else
			{
				rlen += sizeof(int);
			}
		}
	}
	rlen -= rnullFieldLength;


	offset = nownullFieldLength;
	memcpy((char*)data + offset, (char*)ldata + lnullFieldLength, llen);
	offset += llen;
	memcpy((char*)data + offset, (char*)rdata + rnullFieldLength, rlen);
	free(lnullsIndicator);
	free(rnullsIndicator);
}



INLJoin::INLJoin(Iterator *leftIn, IndexScan *rightIn, const Condition &condition)
{
	this->leftIn = leftIn;
	this->rightIn = rightIn;
	this->condition = condition;
	leftIn->getAttributes(lattr);
	rightIn->getAttributes(rattr);
	for (int i = 0; i < lattr.size(); i++)
		joinattr.push_back(lattr[i]);
	for (int i = 0; i < rattr.size(); i++)
		joinattr.push_back(rattr[i]);
	ldata = malloc(ONE_INTERNAL_SIZE);
	frombegin = true;
}
RC INLJoin::getNextTuple(void *data)
{
	void *rdata = malloc(ONE_INTERNAL_SIZE);
	RC rc = 0;
	while (true)
	{
		if (frombegin)
		{
			rc = leftIn->getNextTuple(ldata);
			if (rc == -1) break;
			AttrType tp;
			void *tmp = malloc(ONE_INTERNAL_SIZE);
			int isnull = 0;
			getRattr(lattr, ldata, condition.lhsAttr, tmp, tp, isnull);
			if (tmp == NULL)
			{

				continue;
			}
			else
			{
				rightIn->setIterator(tmp, tmp, true, true);
				frombegin = false;
				free(tmp);
			}
		}

		rc = rightIn->getNextTuple(rdata);
		if (rc != 0) {
			frombegin = true;
		}
		else {
			unsigned int lnullFieldLength = ceil((double)lattr.size() / 8);

			unsigned char *lnullsIndicator = (unsigned char *)malloc(lnullFieldLength);


			unsigned int rnullFieldLength = ceil((double)rattr.size() / 8);

			unsigned char *rnullsIndicator = (unsigned char *)malloc(rnullFieldLength);

			unsigned int nownullFieldLength = ceil((double)(rattr.size() + lattr.size()) / 8);

			memcpy(lnullsIndicator, ldata, lnullFieldLength);
			memcpy(rnullsIndicator, rdata, rnullFieldLength);
			memcpy((char*)data, ldata, lattr.size());
			int offset = lattr.size();
			memcpy((char*)data + offset, rdata, rattr.size());

			int llen = lnullFieldLength;
			for (int i = 0; i < lattr.size(); i++)
			{
				if ((lnullsIndicator[i / 8] & (1 << (7 - i % 8))) == 0) {
					if (lattr[i].type == TypeVarChar)
					{
						int len = 0;
						memcpy(&len, (char*)ldata + llen, sizeof(int));
						llen += sizeof(int) + len;

					}
					else
					{
						llen += sizeof(int);
					}
				}
			}
			llen -= lnullFieldLength;

			int rlen = rnullFieldLength;
			for (int i = 0; i < rattr.size(); i++)
			{
				if ((rnullsIndicator[i / 8] & (1 << (7 - i % 8))) == 0) {
					if (rattr[i].type == TypeVarChar)
					{
						int len = 0;
						memcpy(&len, (char*)rdata + llen, sizeof(int));
						rlen += sizeof(int) + len;

					}
					else
					{
						rlen += sizeof(int);
					}
				}
			}
			rlen -= rnullFieldLength;


			offset = nownullFieldLength;
			memcpy((char*)data + offset, (char*)ldata + lnullFieldLength, llen);
			offset += llen;
			memcpy((char*)data + offset, (char*)rdata + rnullFieldLength, rlen);
			free(rnullsIndicator);
			free(lnullsIndicator);
			break;
		}

	}
	return rc;
}
Aggregate::Aggregate(Iterator *input, Attribute aggAttr, AggregateOp op) {

	Input = input;
	attr = aggAttr;
	OP = op;
	count = 0;
	minv = 999999.0;
	maxv = -999999.0;
	total = 0;
	avg = 0;
	Input->getAttributes(attrs);
	onetime = false;
}
RC Aggregate::getNextTuple(void *data)
{
	if (onetime)
		return -1;


	void *page = malloc(PAGE_SIZE);
	float value;

	while (Input->getNextTuple(page) == 0)
	{
		void * required = malloc(PAGE_SIZE);
		int isnull;
		Input->getRattr(attrs, page, attr.name, required, attr.type, isnull);

		if (attr.type == TypeInt)
		{
			int v;
			memcpy(&v, required, sizeof(int));
			value = (float)v;
			count += 1;
			if (minv > value) minv = value;
			if (maxv < value) maxv = value;
			total += value;
			avg = total / count;
		}
		else if (attr.type == TypeReal)
		{
			memcpy(&value, required, sizeof(float));

			count += 1;
			if (minv > value) minv = value;
			if (maxv < value) maxv = value;
			total += value;
			avg = (float)total / count;
		}
		else
		{
			count += 1.0;

		}

	}
	char ch = 0;
	memcpy(data, &ch, 1);

	if (OP == MIN)
		memcpy((char*)data + 1, &minv, sizeof(float));
	else if (OP == MAX)
		memcpy((char*)data + 1, &maxv, sizeof(float));
	else if (OP == SUM)
		memcpy((char*)data + 1, &total, sizeof(float));
	else if (this->OP == AVG)
		memcpy((char*)data + 1, &avg, sizeof(float));
	else if (this->OP == COUNT)
		memcpy((char*)data + 1, &count, sizeof(float));
	if (!onetime)
	{
		onetime = true;
		return 0;
	}

}
/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2008 The EVEmu Team
	For the latest information visit http://evemu.mmoforge.org
	------------------------------------------------------------------------------------
	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option) any later
	version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place - Suite 330, Boston, MA 02111-1307, USA, or go to
	http://www.gnu.org/copyleft/lesser.txt.
	------------------------------------------------------------------------------------
	Author:		Zhur
*/

#include "EvemuPCH.h"

CharacterDB::CharacterDB(DBcore *db) : ServiceDB(db) {}
CharacterDB::~CharacterDB() {}

PyRepObject *CharacterDB::GetCharacterList(uint32 accountID) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		"SELECT characterID,characterName,0 as deletePrepareDateTime, gender,"
		" accessoryID,beardID,costumeID,decoID,eyebrowsID,eyesID,hairID,"
		" lipstickID,makeupID,skinID,backgroundID,lightID,"
		" headRotation1,headRotation2,headRotation3,eyeRotation1,"
		" eyeRotation2,eyeRotation3,camPos1,camPos2,camPos3,"
		" morph1e,morph1n,morph1s,morph1w,morph2e,morph2n,"
		" morph2s,morph2w,morph3e,morph3n,morph3s,morph3w,"
		" morph4e,morph4n,morph4s,morph4w"
		" FROM character_ "
		" WHERE accountID=%lu", accountID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return NULL;
	}
	
	return(DBResultToRowset(res));
}

bool CharacterDB::ValidateCharName(const char *name)
{
	if (name == NULL || *name == '\0')
		return false;

	if(m_db->IsSafeString(name) == false)
	{
		_log(SERVICE__ERROR, "Name '%s' contains invalid characters.", name);
		return false;
	}
	
	//TODO: should reserve the name, but I don't wanna insert a fake char in order to do it.
	
	DBQueryResult res;
	if(!m_db->RunQuery(res, "SELECT characterID FROM character_ WHERE characterName='%s'", name))
	{
		codelog(SERVICE__ERROR, "Error in query for '%s': %s", name, res.error.c_str());
		return false;
	}

	DBResultRow row;
	if(res.GetRow(row)) {
	   _log(SERVICE__MESSAGE, "Name '%s' is already taken", name);
	   return false; 
	}

	return true;
}

PyRepObject *CharacterDB::GetCharSelectInfo(uint32 characterID) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		"SELECT "
		" characterName AS shortName,bloodlineID,gender,bounty,character_.corporationID,allianceID,title,startDateTime,createDateTime,"
		" securityRating,character_.balance,character_.stationID,solarSystemID,constellationID,regionID,"
		" petitionMessage,logonMinutes,tickerName"
		" FROM character_ "
		"	LEFT JOIN corporation ON character_.corporationID=corporation.corporationID"
		" WHERE characterID=%lu", characterID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return NULL;
	}
	
	return(DBResultToRowset(res));
}

/*
 * This macro checks given CharacterAppearance object (app) if given value (v) is NULL:
 *  if yes, macro evaulates to string "NULL"
 *  if no, macro evaulates to call to function _ToStr, which turns given value to string.
 *
 * This macro is needed when saving CharacterAppearance values into DB (NewCharacter, SaveCharacterAppearance).
 * Resulting value must be const char *.
 */
#define _VoN(app, v) \
	((const char *)(app.IsNull_##v() ? "NULL" : _ToStr(app.Get_##v()).c_str()))

/* a 32 bits unsigned integer can be max 0xFFFFFFFF.
   this results in a text string: '4294967295' which
   is 10 long. Including the '\0' at the end of the
   string it is max 11.
 */
static std::string _ToStr(uint32 v) {
	char buf[11];
	snprintf(buf, 11, "%u", v);
	return(buf);
}

static std::string _ToStr(double v) {
	char buf[32];
	snprintf(buf, 32, "%.13f", v);
	return(buf);
}

InventoryItem *CharacterDB::CreateCharacter2(uint32 accountID, ItemFactory &fact, const CharacterData &data, const CharacterAppearance &app, const CorpMemberInfo &corpData) {
	//do the insert into the entity table to get our char ID.
	ItemData idata(
		data.typeID,
		1, //owner
		data.stationID,
		flagPilot,
		data.name.c_str(),
		GPoint(0, 0, 0)
	);

	InventoryItem *char_item = fact.SpawnItem(idata);
	if(char_item == NULL) {
		codelog(SERVICE__ERROR, "Failed to create character entity!");
		return NULL;
	}

	//set some attributes to char_item
	//use the Set_persist as they are not persistent by default
	char_item->Set_intelligence_persist(data.intelligence);
	char_item->Set_charisma_persist(data.charisma);
	char_item->Set_perception_persist(data.perception);
	char_item->Set_memory_persist(data.memory);
	char_item->Set_willpower_persist(data.willpower);

	std::string nameEsc, titleEsc, descEsc;
	m_db->DoEscapeString(nameEsc, data.name);
	m_db->DoEscapeString(titleEsc, data.title);
	m_db->DoEscapeString(descEsc, data.description);

	DBerror err;

	// Table character_ goes first
	if(!m_db->RunQuery(err,
	"INSERT INTO character_ ("
	"	characterID,characterName,accountID,title,description,typeID,"
	"	createDateTime,startDateTime,"
	"	bounty,balance,securityRating,petitionMessage,logonMinutes,"
	"	corporationID,corporationDateTime,"
	"	stationID,solarSystemID,constellationID,regionID,"
	"	raceID,bloodlineID,ancestryID,careerID,schoolID,careerSpecialityID,gender,"
	"	accessoryID,beardID,costumeID,decoID,eyebrowsID,eyesID,"
	"	hairID,lipstickID,makeupID,skinID,backgroundID,lightID,"
	"	headRotation1,headRotation2,headRotation3,"
	"	eyeRotation1,eyeRotation2,eyeRotation3,"
	"	camPos1,camPos2,camPos3,"
	"	morph1e,morph1n,morph1s,morph1w,"
	"	morph2e,morph2n,morph2s,morph2w,"
	"	morph3e,morph3n,morph3s,morph3w,"
	"	morph4e,morph4n,morph4s,morph4w"
	" ) "
	"VALUES(%lu,'%s',%d,'%s','%s',%lu,"
	"	%llu,%llu,"
	"	%.13f, %.13f, %.13f, '', %lu,"
	"	%ld,%lld,"		//corp
	"	%ld,%ld,%ld,%ld,"	//loc
	"	%lu,%lu,%lu,%lu,%lu,%lu,%lu,"
	"	%s,%s,%ld,%s,%ld,%ld,"
	"	%ld,%s,%s,%ld,%ld,%ld,"
	"	%.13f,%.13f,%.13f,"	//head
	"	%.13f,%.13f,%.13f,"	//eye
	"	%.13f,%.13f,%.13f,"	//cam
	"	%s,%s,%s,%s,"
	"	%s,%s,%s,%s,"
	"	%s,%s,%s,%s,"
	"	%s,%s,%s,%s"
	"	)",
		char_item->itemID(), nameEsc.c_str(), accountID, titleEsc.c_str(), descEsc.c_str(), data.typeID,
		data.createDateTime, data.startDateTime,
		data.bounty,data.balance,data.securityRating,data.logonMinutes,
		data.corporationID, data.corporationDateTime,
		data.stationID, data.solarSystemID, data.constellationID, data.regionID,
		data.raceID, data.bloodlineID, data.ancestryID, data.careerID, data.schoolID, data.careerSpecialityID, data.gender,
		_VoN(app,accessoryID),_VoN(app,beardID),app.costumeID,_VoN(app,decoID),app.eyebrowsID,app.eyesID,
		app.hairID,_VoN(app,lipstickID),_VoN(app,makeupID),app.skinID,app.backgroundID,app.lightID,
		app.headRotation1, app.headRotation2, app.headRotation3, 
		app.eyeRotation1, app.eyeRotation2, app.eyeRotation3,
		app.camPos1, app.camPos2, app.camPos3,
		_VoN(app,morph1e), _VoN(app,morph1n), _VoN(app,morph1s), _VoN(app,morph1w),
		_VoN(app,morph2e), _VoN(app,morph2n), _VoN(app,morph2s), _VoN(app,morph2w),
		_VoN(app,morph3e), _VoN(app,morph3n), _VoN(app,morph3s), _VoN(app,morph3w),
		_VoN(app,morph4e), _VoN(app,morph4n), _VoN(app,morph4s), _VoN(app,morph4w)
	)) {
		codelog(SERVICE__ERROR, "Failed to insert new char %lu: %s", char_item->itemID(), err.c_str());
		char_item->Delete();
		return NULL;
	}

	// Then insert roles into table chrCorporationRoles
	if(!m_db->RunQuery(err,
		"INSERT INTO chrCorporationRoles"
		"  (characterID, corprole, rolesAtAll, rolesAtBase, rolesAtHQ, rolesAtOther)"
		" VALUES"
		"  (%lu, " I64u ", " I64u ", " I64u ", " I64u ", " I64u ")",
		char_item->itemID(), corpData.corprole, corpData.rolesAtAll, corpData.rolesAtBase, corpData.rolesAtHQ, corpData.rolesAtOther))
	{
		_log(DATABASE__ERROR, "Failed to insert corp member info for character %lu: %s.", char_item->itemID(), err.c_str());
		//just let it go... its a lot easier this way
	}

	// Hack in the first employment record
	// TODO: Eventually, this should go under corp stuff...
	if(!m_db->RunQuery(err, 
		"INSERT INTO chrEmployment"
		"  (characterID, corporationID, startDate, deleted)"
		" VALUES"
		"  (%lu, %lu, %llu, 0)",
		char_item->itemID(), data.corporationID, Win32TimeNow()))
	{
		_log(DATABASE__ERROR, "Failed to insert employment info of character %lu: %s.", char_item->itemID(), err.c_str());
		//just let it go... its a lot easier this way
	}

	// And one more member to the corporation
	if(!m_db->RunQuery(err,
		"UPDATE corporation"
		"  SET memberCount = memberCount + 1"
		" WHERE corporationID = %lu",
		data.corporationID))
	{
		_log(DATABASE__ERROR, "Failed to raise member count of corporation %lu: %s.", char_item->itemID(), err.c_str());
		//just let it go... its a lot easier this way
	}

	return(char_item);
}

PyRepObject *CharacterDB::GetCharPublicInfo(uint32 characterID) {
	DBQueryResult res;

	if(!m_db->RunQuery(res,
		"SELECT "
		" character_.typeID,"
		" character_.corporationID,"
        /*" chrBloodlines.raceID,"*/
		" character_.raceID,"
		" character_.bloodlineID,"
		" character_.ancestryID,"
		" character_.careerID,"
		" character_.schoolID,"
		" character_.careerSpecialityID,"
		" character_.characterName,"
		" 0 as age,"	//hack
		" character_.createDateTime,"
		" character_.gender,"
		" character_.characterID,"
		" character_.description,"
		" character_.corporationDateTime"
        " FROM character_ "
        /*"		LEFT JOIN chrBloodlines "
        "		ON character_.bloodlineID=chrBloodlines.bloodlineID"*/
		" WHERE characterID=%lu", characterID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return NULL;
	}
	
	DBResultRow row;
	if(!res.GetRow(row)) {
		_log(SERVICE__ERROR, "Error in GetCharPublicInfo query: no data for char %d", characterID);
		return NULL;
	}
	return(DBRowToKeyVal(row));
	
}

PyRepObject *CharacterDB::GetCharPublicInfo3(uint32 characterID) {

	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		"SELECT "
		" character_.bounty,"
		" character_.title,"
		" character_.startDateTime,"
		" character_.description,"
		" character_.corporationID"
		" FROM character_"
		" WHERE characterID=%lu", characterID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return NULL;
	}
	
	return(DBResultToRowset(res));
}

PyRepObject *CharacterDB::GetCharDesc(uint32 characterID) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		"SELECT "
		" description "
		" FROM character_ "
		" WHERE characterID=%lu", characterID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return NULL;
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		codelog(SERVICE__ERROR, "Error in query: no data for char %d", characterID);
		return NULL;
	}

	return(DBRowToRow(row));
	
}

bool CharacterDB::SetCharDesc(uint32 characterID, const char *str) {
	DBerror err;

	std::string stringEsc;
	m_db->DoEscapeString(stringEsc, str);

	if(!m_db->RunQuery(err,
		"UPDATE character_"
		" SET description ='%s'"
		" WHERE characterID=%lu", stringEsc.c_str(),characterID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", err.c_str());
		return false;
	}

	return (true);
}

//just return all itemIDs which has ownerID set to characterID
bool CharacterDB::GetCharItems(uint32 characterID, std::vector<uint32> &into) {
	DBQueryResult res;

	if(!m_db->RunQuery(res,
		"SELECT"
		"  itemID"
		" FROM entity"
		" WHERE ownerID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to query items of char %lu: %s.", characterID, res.error.c_str());
		return false;
	}

	DBResultRow row;
	while(res.GetRow(row))
		into.push_back(row.GetUInt(0));
	
	return true;
}

//Try to run through all of the tables which might have data relavent to
//this person in it, and run a delete against it.
bool CharacterDB::DeleteCharacter(uint32 characterID) {
	DBerror err;

	// eveMailDetails
	if(!m_db->RunQuery(err,
		"DELETE FROM eveMailDetails"
		"  USING eveMail, eveMailDetails"
		" WHERE"
		"   eveMail.messageID = eveMailDetails.messageID"
		"  AND"
		"   (senderID = %lu OR channelID = %lu)",
		characterID, characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete mail details of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// eveMail
	if(!m_db->RunQuery(err,
		"DELETE FROM eveMail"
		" WHERE (senderID = %lu OR channelID = %lu)",
		characterID, characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete mail of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// crpCharShares
	if(!m_db->RunQuery(err,
		"DELETE FROM crpCharShares"
		" WHERE characterID = %lu",
		characterID))
	{
		codelog(SERVICE__ERROR, "Failed to delete shares of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// bookmarks
	if(!m_db->RunQuery(err,
		"DELETE FROM bookmarks"
		" WHERE ownerID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete bookmarks of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// market_journal
	if(!m_db->RunQuery(err,
		"DELETE FROM market_journal"
		" WHERE characterID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete market journal of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// market_orders
	if(!m_db->RunQuery(err,
		"DELETE FROM market_orders"
		" WHERE charID =% lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete market orders of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// market_transactions
	if(!m_db->RunQuery(err,
		"DELETE FROM market_transactions"
		" WHERE clientID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete market transactions of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// chrStandings
	if(!m_db->RunQuery(err,
		"DELETE FROM chrStandings"
		" WHERE characterID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete standings of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// chrNPCStandings
	if(!m_db->RunQuery(err,
		"DELETE FROM chrNPCStandings"
		" WHERE characterID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete NPC standings of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// chrCorporationRoles
	if(!m_db->RunQuery(err,
		"DELETE FROM chrCorporationRoles"
		" WHERE characterID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete corporation roles of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// chrEmployment
	if(!m_db->RunQuery(err,
		"DELETE FROM chrEmployment"
		" WHERE characterID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete employment of character %lu: %s.", characterID, err.c_str());
		// ignore the error
		_log(DATABASE__MESSAGE, "Ignoring error.");
	}

	// character_
	if(!m_db->RunQuery(err,
		"DELETE FROM character_"
		" WHERE characterID = %lu",
		characterID))
	{
		_log(DATABASE__ERROR, "Failed to delete character %lu: %s.", characterID, err.c_str());
		return false;
	}

	return true;
}

PyRepObject *CharacterDB::GetCharacterAppearance(uint32 charID) {
	DBQueryResult res;
	
	if(!m_db->RunQuery(res,
		"SELECT "
		" accessoryID,beardID,costumeID,decoID,eyebrowsID,eyesID,hairID,"
		" lipstickID,makeupID,skinID,backgroundID,lightID,"
		" headRotation1,headRotation2,headRotation3,eyeRotation1,"
		" eyeRotation2,eyeRotation3,camPos1,camPos2,camPos3,"
		" morph1e,morph1n,morph1s,morph1w,morph2e,morph2n,"
		" morph2s,morph2w,morph3e,morph3n,morph3s,morph3w,"
		" morph4e,morph4n,morph4s,morph4w"
		" FROM character_ "
		" WHERE characterID=%lu", charID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return NULL;
	}

	return(DBResultToRowset(res));
}

bool CharacterDB::GetInfoByBloodline(CharacterData &cdata, uint32 &shipTypeID) {
	DBQueryResult res;

	if(!m_db->RunQuery(res,
		"SELECT"
		"  bloodlineTypes.typeID,"
		"  chrBloodlines.raceID,"
		"  chrBloodlines.shipTypeID"
		" FROM chrBloodlines"
		"  LEFT JOIN bloodlineTypes USING (bloodlineID)"
		" WHERE bloodlineID = %lu",
		cdata.bloodlineID))
	{
		_log(DATABASE__ERROR, "Failed to query bloodline %lu: %s.", cdata.bloodlineID, res.error.c_str());
		return false;
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		_log(DATABASE__ERROR, "No bloodline %lu found.", cdata.bloodlineID);
		return false;
	}

	cdata.typeID = row.GetUInt(0);
	cdata.raceID = row.GetUInt(1);

	shipTypeID = row.GetUInt(2);

	return true;
}

bool CharacterDB::GetAttributesFromBloodline(CharacterData & cdata) {
	DBQueryResult res;

	if (!m_db->RunQuery(res,
		" SELECT "
		"  intelligence, charisma, perception, memory, willpower "
		" FROM chrBloodlines "
		" WHERE chrBloodlines.bloodlineID = %lu ", cdata.bloodlineID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return (false);
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		codelog(SERVICE__ERROR, "Failed to find bloodline information for bloodline %lu", cdata.bloodlineID);
		return false;
	}

	cdata.intelligence += row.GetUInt(0);
	cdata.charisma += row.GetUInt(1);
	cdata.perception += row.GetUInt(2);
	cdata.memory += row.GetUInt(3);
	cdata.willpower += row.GetUInt(4);
	
	return (true);
}

bool CharacterDB::GetAttributesFromAncestry(CharacterData & cdata) {
	DBQueryResult res;

	if (!m_db->RunQuery(res,
		" SELECT "
		"        intelligence, charisma, perception, memory, willpower "
		" FROM chrAncestries "
		" WHERE ancestryID = %lu ", cdata.ancestryID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return (false);
	}

	DBResultRow row;
	if(!res.GetRow(row)) {
		codelog(SERVICE__ERROR, "Failed to find ancestry information for ancestry %lu", cdata.ancestryID);
		return false;
	}
	
	cdata.intelligence += row.GetUInt(0);
	cdata.charisma += row.GetUInt(1);
	cdata.perception += row.GetUInt(2);
	cdata.memory += row.GetUInt(3);
	cdata.willpower += row.GetUInt(4);

	return (true);
}

/**
  * @todo Here should come a call to Corp??::CharacterJoinToCorp or what the heck... for now we only put it there
  */
bool CharacterDB::GetLocationCorporationByCareer(CharacterData &cdata) {
	DBQueryResult res;
	if (!m_db->RunQuery(res, 
	 "SELECT "
	 "  chrSchools.corporationID, "
	 "  chrSchools.schoolID, "
	 "  corporation.allianceID, "
	 "  corporation.stationID, "
	 "  staStations.solarSystemID, "
	 "  staStations.constellationID, "
	 "  staStations.regionID "
	 " FROM staStations"
	 "  LEFT JOIN corporation ON corporation.stationID=staStations.stationID"
	 "  LEFT JOIN chrSchools ON corporation.corporationID=chrSchools.corporationID"
	 "  LEFT JOIN chrCareers ON chrSchools.careerID=chrCareers.careerID"
	 " WHERE chrCareers.careerID = %lu", cdata.careerID))
	{
		codelog(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return (false);
	}
	
	DBResultRow row;
	if(!res.GetRow(row)) {
		codelog(SERVICE__ERROR, "Failed to find career %lu", cdata.careerID);
		return false;
	}
	
	cdata.corporationID = row.GetUInt(0);
	cdata.schoolID = row.GetUInt(1);
	cdata.allianceID = row.GetUInt(2);

	cdata.stationID = row.GetUInt(3);
	cdata.solarSystemID = row.GetUInt(4);
	cdata.constellationID = row.GetUInt(5);
	cdata.regionID = row.GetUInt(6);

	return (true);
}

bool CharacterDB::GetSkillsByRace(uint32 raceID, std::map<uint32, uint32> &into) {
	DBQueryResult res;

	if (!m_db->RunQuery(res,
		"SELECT "
		"        skillTypeID, levels"
		" FROM chrRaceSkills "
		" WHERE raceID = %lu ", raceID))
	{
		_log(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return false;
	}

	DBResultRow row;
	while(res.GetRow(row)) {
		if(into.find(row.GetUInt(0)) == into.end())
			into[row.GetUInt(0)] = row.GetUInt(1);
		else
			into[row.GetUInt(0)] += row.GetUInt(1);
		//check to avoid more than 5 levels by skill
		if(into[row.GetUInt(0)] > 5)
			into[row.GetUInt(0)] = 5;
	}

	return true;
}

bool CharacterDB::GetSkillsByCareer(uint32 careerID, std::map<uint32, uint32> &into) {
	DBQueryResult res;

	if (!m_db->RunQuery(res,
		"SELECT "
		"        skillTypeID, levels"
		" FROM chrCareerSkills"
		" WHERE careerID = %lu", careerID))
	{
		_log(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return false;
	}
	
	DBResultRow row;
	while(res.GetRow(row)) {
		if(into.find(row.GetUInt(0)) == into.end())
			into[row.GetUInt(0)] = row.GetUInt(1);
		else
			into[row.GetUInt(0)] += row.GetUInt(1);
		//check to avoid more than 5 levels by skill
		if(into[row.GetUInt(0)] > 5)
			into[row.GetUInt(0)] = 5;
	}

	return true;
}

bool CharacterDB::GetSkillsByCareerSpeciality(uint32 careerSpecialityID, std::map<uint32, uint32> &into) {
	DBQueryResult res;

	if (!m_db->RunQuery(res,
		"SELECT "
		"        skillTypeID, levels"
		" FROM chrCareerSpecialitySkills"
		" WHERE specialityID = %lu", careerSpecialityID))
	{
		_log(SERVICE__ERROR, "Error in query: %s", res.error.c_str());
		return false;
	}
	
	DBResultRow row;
	while(res.GetRow(row)) {
		if(into.find(row.GetUInt(0)) == into.end())
			into[row.GetUInt(0)] = row.GetUInt(1);
		else
			into[row.GetUInt(0)] += row.GetUInt(1);
		//check to avoid more than 5 levels by skill
		if(into[row.GetUInt(0)] > 5)
			into[row.GetUInt(0)] = 5;
	}

	return true;
}

PyRepString *CharacterDB::GetNote(uint32 ownerID, uint32 itemID) {
	DBQueryResult res;

	if (!m_db->RunQuery(res,
			"SELECT `note` FROM `chrNotes` WHERE ownerID = %lu AND itemID = %lu",
			ownerID, itemID)
		)
	{
		codelog(SERVICE__ERROR, "Error on query: %s", res.error.c_str());
		return NULL;
	}
	DBResultRow row;
	if(!res.GetRow(row))
		return NULL;

	return(new PyRepString(row.GetText(0)));
}

bool CharacterDB::SetNote(uint32 ownerID, uint32 itemID, const char *str) {
	DBerror err;

	if (str[0] == '\0') {
		// str is empty
		if (!m_db->RunQuery(err,
			"DELETE FROM `chrNotes` "
			" WHERE itemID = %lu AND ownerID = %lu LIMIT 1",
			ownerID, itemID)
			)
		{
			codelog(CLIENT__ERROR, "Error on query: %s", err.c_str());
			return false;
		}
	} else {
		// escape it before insertion
		std::string escaped;
		m_db->DoEscapeString(escaped, str);

		if (!m_db->RunQuery(err,
			"REPLACE INTO `chrNotes` (itemID, ownerID, note)	"
			"VALUES (%lu, %lu, '%s')",
			ownerID, itemID, escaped.c_str())
			)
		{
			codelog(CLIENT__ERROR, "Error on query: %s", err.c_str());
			return false;
		}
	}

	return true;
}

uint32 CharacterDB::AddOwnerNote(uint32 charID, const std::string & label, const std::string & content) {
	DBerror err;
	uint32 id;

	std::string lblS;
	m_db->DoEscapeString(lblS, label);

	std::string contS;
	m_db->DoEscapeString(contS, content);

	if (!m_db->RunQueryLID(err, id, 
		"INSERT INTO chrOwnerNote (ownerID, label, note) VALUES (%lu, '%s', '%s');",
		charID, lblS.c_str(), contS.c_str()))
	{
		codelog(SERVICE__ERROR, "Error on query: %s", err.c_str());
		return 0;
	}

	return id;
}

bool CharacterDB::EditOwnerNote(uint32 charID, uint32 noteID, const std::string & label, const std::string & content) {
	DBerror err;

	std::string contS;
	m_db->DoEscapeString(contS, content);

	if (!m_db->RunQuery(err,
		"UPDATE chrOwnerNote SET note = '%s' WHERE ownerID = %lu AND noteID = %lu;",
		contS.c_str(), charID, noteID))
	{
		codelog(SERVICE__ERROR, "Error on query: %s", err.c_str());
		return false;
	}

	return true;
}

PyRepObject *CharacterDB::GetOwnerNoteLabels(uint32 charID) {
	DBQueryResult res;

	if (!m_db->RunQuery(res, "SELECT noteID, label FROM chrOwnerNote WHERE ownerID = %lu", charID))
	{
		codelog(SERVICE__ERROR, "Error on query: %s", res.error.c_str());
		return (NULL);
	}

	return DBResultToRowset(res);
}

PyRepObject *CharacterDB::GetOwnerNote(uint32 charID, uint32 noteID) {
	DBQueryResult res;

	if (!m_db->RunQuery(res, "SELECT note FROM chrOwnerNote WHERE ownerID = %lu AND noteID = %lu", charID, noteID))
	{
		codelog(SERVICE__ERROR, "Error on query: %s", res.error.c_str());
		return (NULL);
	}

	return DBResultToRowset(res);
}









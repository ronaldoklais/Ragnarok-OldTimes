// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

// original code from athena
// SQL conversion by Jioh L. Jung
// TXT 1.105
#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
typedef long in_addr_t;
//#pragma lib <libmysql.lib> // Not required [Lance]
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "char.h"
#include "../common/utils.h"
#include "../common/strlib.h"
#include "../common/showmsg.h"
#include "itemdb.h"
#include "inter.h"
#include "db.h"
#include "malloc.h"
#include "int_guild.h"

static struct dbt *char_db_;

char char_db[256] = "char";
char scdata_db[256] = "sc_data";
char cart_db[256] = "cart_inventory";
char inventory_db[256] = "inventory";
char charlog_db[256] = "charlog";
char storage_db[256] = "storage";
char interlog_db[256] = "interlog";
char reg_db[256] = "global_reg_value";
char skill_db[256] = "skill";
char memo_db[256] = "memo";
char guild_db[256] = "guild";
char guild_alliance_db[256] = "guild_alliance";
char guild_castle_db[256] = "guild_castle";
char guild_expulsion_db[256] = "guild_expulsion";
char guild_member_db[256] = "guild_member";
char guild_position_db[256] = "guild_position";
char guild_skill_db[256] = "guild_skill";
char guild_storage_db[256] = "guild_storage";
char party_db[256] = "party";
char pet_db[256] = "pet";
char friend_db[256] = "friends";
int db_use_sqldbs;

char login_db_account_id[32] = "account_id";
char login_db_level[32] = "level";

int lowest_gm_level = 1;

char *SQL_CONF_NAME = "conf/inter_athena.conf";

struct mmo_map_server server[MAX_MAP_SERVERS];
int server_fd[MAX_MAP_SERVERS];

int login_fd, char_fd;
char userid[24];
char passwd[24];
char server_name[20];
char wisp_server_name[NAME_LENGTH] = "Server";
int login_ip_set_ = 0;
char login_ip_str[128];
in_addr_t login_ip;
int login_port = 6900;
int char_ip_set_ = 0;
char char_ip_str[128];
int bind_ip_set_ = 0;
char bind_ip_str[128];
in_addr_t char_ip;
int char_port = 6121;
int char_maintenance;
int char_new;
int char_new_display;
int name_ignoring_case = 0; // Allow or not identical name for characters but with a different case by [Yor]
int char_name_option = 0; // Option to know which letters/symbols are authorised in the name of a character (0: all, 1: only those in char_name_letters, 2: all EXCEPT those in char_name_letters) by [Yor]
char char_name_letters[1024] = ""; // list of letters/symbols used to authorise or not a name of a character. by [Yor]
//The following are characters that are trimmed regardless because they cause confusion and problems on the servers. [Skotlex]
#define TRIM_CHARS "\032\t\n "
int char_per_account = 0; //Maximum charas per account (default unlimited) [Sirius]

int log_char = 1;	// loggin char or not [devil]
int log_inter = 1;	// loggin inter or not [devil]

char lan_map_ip[128]; // Lan map ip added by kashy
int subnetmaski[4]; // Subnetmask added by kashy
char unknown_char_name[NAME_LENGTH] = "Unknown";
char db_path[1024]="db";

//These are used to aid the map server in identifying valid clients. [Skotlex]
static int max_account_id = DEFAULT_MAX_ACCOUNT_ID, max_char_id = DEFAULT_MAX_CHAR_ID;
static int online_check = 1; //If one, it won't let players connect when their account is already registered online and will send the relevant map server a kick user request. [Skotlex]

struct char_session_data{
	int account_id, login_id1, login_id2,sex;
	int found_char[9];
	char email[40]; // e-mail (default: a@a.com) by [Yor]
	time_t connect_until_time; // # of seconds 1/1/1970 (timestamp): Validity limit of the account (0 = unlimited)
};

#define AUTH_FIFO_SIZE 256
struct {
	int account_id, char_id, login_id1, login_id2, ip, char_pos, delflag,sex;
	time_t connect_until_time; // # of seconds 1/1/1970 (timestamp): Validity limit of the account (0 = unlimited)
} auth_fifo[AUTH_FIFO_SIZE];
int auth_fifo_pos = 0;

int check_ip_flag = 1; // It's to check IP of a player between char-server and other servers (part of anti-hacking system)

int char_id_count = START_CHAR_NUM;
struct mmo_charstatus *char_dat;
int char_num,char_max;
int max_connect_user = 0;
int gm_allow_level = 99;
int autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;
int save_log = 1;
int start_zeny = 500;
int start_weapon = 1201;
int start_armor = 2301;

// check for exit signal
// 0 is saving complete
// other is char_id
unsigned int save_flag = 0;

// start point (you can reset point on conf file)
struct point start_point = {"new_1-1.gat", 53, 111};

struct gm_account *gm_account = NULL;
int GM_num = 0;

int console = 0;

//Structure for holding in memory which characters are online on the map servers connected.
struct online_char_data {
	int account_id;
	int char_id;
	short server;
	unsigned waiting_disconnect :1;
};

struct dbt *online_char_db; //Holds all online characters.

#ifndef SQL_DEBUG

#define mysql_query(_x, _y) mysql_real_query(_x, _y, strlen(_y)) //supports ' in names and runs faster [Kevin]

#else

#define mysql_query(_x, _y) debug_mysql_query(__FILE__, __LINE__, _x, _y)

#endif

static int chardb_waiting_disconnect(int tid, unsigned int tick, int id, int data);

//-------------------------------------------------
// Set Character online/offline [Wizputer]
//-------------------------------------------------

void set_char_online(int map_id, int char_id, int account_id) {
	struct online_char_data* character;
	if ( char_id != 99 ) {
		sprintf(tmp_sql, "UPDATE `%s` SET `online`='1' WHERE `char_id`='%d'",char_db,char_id);
		if (mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}

		if (max_account_id < account_id || max_char_id < char_id)
		{	//Notify map-server of the new max IDs [Skotlex]
			if (account_id > max_account_id)
				max_account_id = account_id;
			if (char_id > max_char_id)
				max_char_id = char_id;
			mapif_send_maxid(max_account_id, max_char_id);
		}
	}

	character = numdb_search(online_char_db, account_id);
	if (character == NULL)
	{
		character = aCalloc(1, sizeof(struct online_char_data));
		character->account_id = account_id;
		numdb_insert(online_char_db, account_id, character);
	} else {
		if (online_check && character->char_id != -1 && character->server > -1 && character->server != map_id)
		{
			ShowNotice("set_char_online: Character %d:%d marked in map server %d, but map server %d claims to have (%d:%d) online!\n",
				character->account_id, character->char_id, character->server, map_id, account_id, char_id);
			mapif_disconnectplayer(server_fd[character->server], character->account_id, character->char_id, 2);
		}
		character->waiting_disconnect = 0;
	}
	character->char_id = (char_id==99)?-1:char_id;
	character->server = (char_id==99)?-1:map_id;
	if (char_id != 99)
	{	//Set char online in guild cache. If char is in memory, use the guild id on it, otherwise seek it.
		struct mmo_charstatus *cp;
		cp = (struct mmo_charstatus*)numdb_search(char_db_,char_id);
 		inter_guild_CharOnline(char_id, cp?cp->guild_id:-1);
	}
	if (login_fd <= 0 || session[login_fd]->eof)
		return;

	WFIFOW(login_fd,0) = 0x272b;
	WFIFOL(login_fd,2) = account_id;
	WFIFOSET(login_fd,6);
}

void set_char_offline(int char_id, int account_id) {
	struct mmo_charstatus *cp;
	struct online_char_data* character;

	if ( char_id == 99 )
		sprintf(tmp_sql,"UPDATE `%s` SET `online`='0' WHERE `account_id`='%d'", char_db, account_id);
	else {
		cp = (struct mmo_charstatus*)numdb_search(char_db_,char_id);
		inter_guild_CharOffline(char_id, cp?cp->guild_id:-1);
		if (cp != NULL) {
			aFree(cp);
			numdb_erase(char_db_,char_id);
		}

		sprintf(tmp_sql,"UPDATE `%s` SET `online`='0' WHERE `char_id`='%d'", char_db, char_id);

		if (mysql_query(&mysql_handle, tmp_sql))
		{
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
	}

	if ((character = numdb_search(online_char_db, account_id)) != NULL)
	{	//We don't free yet to avoid aCalloc/aFree spamming during char change. [Skotlex]
		character->char_id = -1;
		character->server = -1;
		character->waiting_disconnect = 0;
	}

   if (login_fd <= 0 || session[login_fd]->eof)
	return;

   WFIFOW(login_fd,0) = 0x272c;
   WFIFOL(login_fd,2) = account_id;
   WFIFOSET(login_fd,6);
}

static int char_db_setoffline(void* key, void* data, va_list ap) {
	struct online_char_data* character = (struct online_char_data*)data;
	int server = va_arg(ap, int);
	if (server == -1) {
		character->char_id = -1;
		character->server = -1;
		character->waiting_disconnect = 0;
	} else if (character->server == server)
		character->server = -2; //In some map server that we aren't connected to.
	return 0;
}

void set_all_offline(void) {
	MYSQL_RES*       sql_res2; //Needed because it is used inside inter_guild_CharOffline; [Skotlex]
	int char_id;
	sprintf(tmp_sql, "SELECT `account_id`, `char_id`, `guild_id` FROM `%s` WHERE `online`='1'",char_db);
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	ShowNotice("Sending all users offline.\n");
	sql_res2 = mysql_store_result(&mysql_handle);
	if (sql_res2) {
		while((sql_row = mysql_fetch_row(sql_res2))) {
			char_id = atoi(sql_row[1]);
			inter_guild_CharOffline(char_id, atoi(sql_row[2]));

			if ( login_fd > 0 ) {
				ShowInfo("send user offline: %d\n",char_id);
				WFIFOW(login_fd,0) = 0x272c;
				WFIFOL(login_fd,2) = char_id;
				WFIFOSET(login_fd,6);
			}
		}
		mysql_free_result(sql_res2);
	}

	sprintf(tmp_sql,"UPDATE `%s` SET `online`='0' WHERE `online`='1'", char_db);
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	numdb_foreach(online_char_db,char_db_setoffline,-1);
}
//----------------------------------------------------------------------
// Determine if an account (id) is a GM account
// and returns its level (or 0 if it isn't a GM account or if not found)
//----------------------------------------------------------------------
// Removed since nothing GM related goes on in the char server [CLOWNISIUS]
int isGM(int account_id) {
	int i;

	for(i = 0; i < GM_num; i++)
		if (gm_account[i].account_id == account_id)
			return gm_account[i].level;
	return 0;
}


int compare_item(struct item *a, struct item *b) {

	if(a->id == b->id &&
		a->nameid == b->nameid &&
		a->amount == b->amount &&
		a->equip == b->equip &&
		a->identify == b->identify &&
		a->refine == b->refine &&
		a->attribute == b->attribute)
	{
		int i;
		for (i=0; i<MAX_SLOTS && a->card[i]==b->card[i]; i++);
		return (i == MAX_SLOTS);
	}
	return 0;
}

int mmo_char_tosql(int char_id, struct mmo_charstatus *p){
	int i=0,j,party_exist,guild_exist;
	int count = 0;
	int diff = 0;
	char temp_str[64]; //2x the value of the string before jstrescapecpy [Skotlex]
	char temp_str2[512];
	char *tmp_ptr; //Building a single query should be more efficient than running
		//multiple queries for each thing about to be saved, right? [Skotlex]
	char save_status[128]; //For displaying save information. [Skotlex]
	struct mmo_charstatus *cp;
	struct itemtmp mapitem[MAX_GUILD_STORAGE];

	if (char_id!=p->char_id) return 0;

	cp = (struct mmo_charstatus*)numdb_search(char_db_,char_id);

	if (cp == NULL) {
		cp = (struct mmo_charstatus *) aMalloc(sizeof(struct mmo_charstatus));
    		memset(cp, 0, sizeof(struct mmo_charstatus));
		numdb_insert(char_db_, char_id,cp);
	}

//	ShowInfo("Saving char "CL_WHITE"%d"CL_RESET" (%s)...\n",char_id,char_dat[0].name);
	memset(save_status, 0, sizeof(save_status));
	diff = 0;
	//map inventory data
	for(i=0;i<MAX_INVENTORY;i++){
		if (!compare_item(&p->inventory[i], &cp->inventory[i]))
			diff = 1;
		if(p->inventory[i].nameid>0){
			mapitem[count].flag=0;
			mapitem[count].id = p->inventory[i].id;
			mapitem[count].nameid=p->inventory[i].nameid;
			mapitem[count].amount = p->inventory[i].amount;
			mapitem[count].equip = p->inventory[i].equip;
			mapitem[count].identify = p->inventory[i].identify;
			mapitem[count].refine = p->inventory[i].refine;
			mapitem[count].attribute = p->inventory[i].attribute;
			for (j=0; j<MAX_SLOTS; j++)
				mapitem[count].card[j] = p->inventory[i].card[j];
			count++;
		}
	}
	//printf("- Save item data to MySQL!\n");
	if (diff)
		if (!memitemdata_to_sql(mapitem, count, p->char_id,TABLE_INVENTORY))
			strcat(save_status, " inventory");

	count = 0;
	diff = 0;

	//map cart data
	for(i=0;i<MAX_CART;i++){
		if (!compare_item(&p->cart[i], &cp->cart[i]))
			diff = 1;
		if(p->cart[i].nameid>0){
			mapitem[count].flag=0;
			mapitem[count].id = p->cart[i].id;
			mapitem[count].nameid=p->cart[i].nameid;
			mapitem[count].amount = p->cart[i].amount;
			mapitem[count].equip = p->cart[i].equip;
			mapitem[count].identify = p->cart[i].identify;
			mapitem[count].refine = p->cart[i].refine;
			mapitem[count].attribute = p->cart[i].attribute;
			for (j=0; j<MAX_SLOTS; j++)
				mapitem[count].card[j] = p->cart[i].card[j];
			count++;
		}
	}

	if (diff)
		if (!memitemdata_to_sql(mapitem, count, p->char_id,TABLE_CART))
			strcat(save_status, " cart");

	if (
		(p->base_exp != cp->base_exp) || (p->base_level != cp->base_level) ||
		(p->job_level != cp->job_level) || (p->job_exp != cp->job_exp) ||
		(p->zeny != cp->zeny) ||
		(p->last_point.x != cp->last_point.x) || (p->last_point.y != cp->last_point.y) ||
		(p->max_hp != cp->max_hp) || (p->hp != cp->hp) ||
		(p->max_sp != cp->max_sp) || (p->sp != cp->sp) ||
		(p->status_point != cp->status_point) || (p->skill_point != cp->skill_point) ||
		(p->str != cp->str) || (p->agi != cp->agi) || (p->vit != cp->vit) ||
		(p->int_ != cp->int_) || (p->dex != cp->dex) || (p->luk != cp->luk) ||
		(p->option != cp->option) ||
		(p->party_id != cp->party_id) || (p->guild_id != cp->guild_id) ||
		(p->pet_id != cp->pet_id) || (p->weapon != cp->weapon) ||
		(p->shield != cp->shield) || (p->head_top != cp->head_top) ||
		(p->head_mid != cp->head_mid) || (p->head_bottom != cp->head_bottom)
	)
	{	//Save status
		//Check for party
		party_exist=1;
		sprintf(tmp_sql, "SELECT count(*) FROM `%s` WHERE `party_id` = '%d'",party_db, p->party_id); // TBR
		if (mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		} else { //In case of failure, don't touch the data. [Skotlex
			sql_res = mysql_store_result(&mysql_handle);
			sql_row = sql_res?mysql_fetch_row(sql_res):NULL;
			if (sql_row)
				party_exist = atoi(sql_row[0]);
			mysql_free_result(sql_res);
		}

		//check guild_exist
		guild_exist=1;
		sprintf(tmp_sql, "SELECT count(*) FROM `%s` WHERE `guild_id` = '%d'",guild_db, p->guild_id); // TBR
		if (mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		} else { //If we fail to confirm, don't touch the data.
			sql_res = mysql_store_result(&mysql_handle);
			sql_row = sql_res?mysql_fetch_row(sql_res):NULL;
			if (sql_row)
				guild_exist = atoi(sql_row[0]);
			mysql_free_result(sql_res);
		}

		if (guild_exist==0) p->guild_id=0;
		if (party_exist==0) p->party_id=0;

		//query
		sprintf(tmp_sql ,"UPDATE `%s` SET `base_level`='%d', `job_level`='%d',"
			"`base_exp`='%d', `job_exp`='%d', `zeny`='%d',"
			"`max_hp`='%d',`hp`='%d',`max_sp`='%d',`sp`='%d',`status_point`='%d',`skill_point`='%d',"
			"`str`='%d',`agi`='%d',`vit`='%d',`int`='%d',`dex`='%d',`luk`='%d',"
			"`option`='%d',`party_id`='%d',`guild_id`='%d',`pet_id`='%d',"
			"`weapon`='%d',`shield`='%d',`head_top`='%d',`head_mid`='%d',`head_bottom`='%d',"
			"`last_map`='%s',`last_x`='%d',`last_y`='%d',`save_map`='%s',`save_x`='%d',`save_y`='%d'"
			" WHERE  `account_id`='%d' AND `char_id` = '%d'",
			char_db, p->base_level, p->job_level,
			p->base_exp, p->job_exp, p->zeny,
			p->max_hp, p->hp, p->max_sp, p->sp, p->status_point, p->skill_point,
			p->str, p->agi, p->vit, p->int_, p->dex, p->luk,
			p->option, p->party_id, p->guild_id, p->pet_id,
			p->weapon, p->shield, p->head_top, p->head_mid, p->head_bottom,
			p->last_point.map, p->last_point.x, p->last_point.y,
			p->save_point.map, p->save_point.x, p->save_point.y,
			p->account_id, p->char_id
		);

		if(mysql_query(&mysql_handle, tmp_sql))
		{
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		} else
			strcat(save_status, " status");
	}

	//Values that will seldom change (to speed up saving)
	if (
		(p->hair != cp->hair) || (p->hair_color != cp->hair_color) || (p->clothes_color != cp->clothes_color) ||
		(p->class_ != cp->class_) ||
		(p->partner_id != cp->partner_id) || (p->father != cp->father) ||
		(p->mother != cp->mother) || (p->child != cp->child) ||
 		(p->karma != cp->karma) || (p->manner != cp->manner) ||
		(p->fame != cp->fame)
	)
	{
		sprintf(tmp_sql ,"UPDATE `%s` SET `class`='%d',"
			"`hair`='%d',`hair_color`='%d',`clothes_color`='%d',"
			"`partner_id`='%d', `father`='%d', `mother`='%d', `child`='%d',"
			"`karma`='%d',`manner`='%d', `fame`='%d'"
			" WHERE  `account_id`='%d' AND `char_id` = '%d'",
			char_db, p->class_,
			p->hair, p->hair_color, p->clothes_color,
			p->partner_id, p->father, p->mother, p->child,
			p->karma, p->manner, p->fame,
			p->account_id, p->char_id
		);
		if(mysql_query(&mysql_handle, tmp_sql))
		{
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		} else
			strcat(save_status, " status2");
	}


	diff = 0;

	for(i=0;i<MAX_MEMOPOINTS;i++){
		if((strcmp(p->memo_point[i].map,cp->memo_point[i].map) == 0) && (p->memo_point[i].x == cp->memo_point[i].x) && (p->memo_point[i].y == cp->memo_point[i].y))
			continue;
		diff = 1;
		break;
	}

	if (diff)
	{ //Save memo
		//`memo` (`memo_id`,`char_id`,`map`,`x`,`y`)
		sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",memo_db, p->char_id);
		if(mysql_query(&mysql_handle, tmp_sql))
		{
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}

		//insert here.
		tmp_ptr = tmp_sql;
		tmp_ptr += sprintf(tmp_ptr, "INSERT INTO `%s`(`char_id`,`map`,`x`,`y`) VALUES ", memo_db);
		count = 0;
		for(i=0;i<MAX_MEMOPOINTS;i++){
			if(p->memo_point[i].map[0]){
				tmp_ptr += sprintf(tmp_ptr,"('%d', '%s', '%d', '%d'),",
					char_id, p->memo_point[i].map, p->memo_point[i].x, p->memo_point[i].y);
				count++;
			}
		}
		if (count)
		{	//Dangerous? Only if none of the above sprintf worked. [Skotlex]
			tmp_ptr[-1] = '\0'; //Remove the trailing comma.
			if(mysql_query(&mysql_handle, tmp_sql))
			{
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			} else
				strcat(save_status, " memo");
		} else //Memo Points cleared (how is this possible?).
			strcat(save_status, " memo");
	}

	diff = 0;
	for(i=0;i<MAX_SKILL;i++) {
		if ((p->skill[i].lv != 0) && (p->skill[i].id == 0))
			p->skill[i].id = i; // Fix skill tree

		if((p->skill[i].id != cp->skill[i].id) || (p->skill[i].lv != cp->skill[i].lv) ||
			(p->skill[i].flag != cp->skill[i].flag))
		{
			diff = 1;
			break;
		}
	}

	if (diff)
	{	//Save skills

		//`skill` (`char_id`, `id`, `lv`)
		sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",skill_db, p->char_id);
		if(mysql_query(&mysql_handle, tmp_sql))
		{
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
		tmp_ptr = tmp_sql;
		tmp_ptr += sprintf(tmp_ptr,"INSERT INTO `%s`(`char_id`,`id`,`lv`) VALUES ", skill_db);
		count = 0;
		//insert here.
		for(i=0;i<MAX_SKILL;i++){
			if(p->skill[i].id && p->skill[i].flag!=1)
			{
				tmp_ptr += sprintf(tmp_ptr,"('%d','%d','%d'),",
					char_id, p->skill[i].id, (p->skill[i].flag==0)?p->skill[i].lv:p->skill[i].flag-2);
				count++;
			}
		}

		if (count)
		{
			tmp_ptr[-1] = '\0';	//Remove trailing comma.
			if(mysql_query(&mysql_handle, tmp_sql))
			{
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			} else
				strcat(save_status, " skills");
		} else //Skills removed (reset?)
			strcat(save_status, " skills");
	}

	diff = 0;
	for(i=0;i<p->global_reg_num;i++) {
		if ((p->global_reg[i].str == NULL) && (cp->global_reg[i].str == NULL))
			continue;
		if (((p->global_reg[i].str == NULL) != (cp->global_reg[i].str == NULL)) ||
			strcmp(p->global_reg[i].value, cp->global_reg[i].value) != 0 ||
			strcmp(p->global_reg[i].str, cp->global_reg[i].str) != 0
		) {
			diff = 1;
			break;
		}
	}

	if (diff)
	{	//Save global registry.
		//`global_reg_value` (`char_id`, `str`, `value`)

		sprintf(tmp_sql,"DELETE FROM `%s` WHERE `type`=3 AND `char_id`='%d'",reg_db, p->char_id);
		if (mysql_query(&mysql_handle, tmp_sql))
		{
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}

		//insert here.
		tmp_ptr = tmp_sql;
		tmp_ptr += sprintf(tmp_ptr,"INSERT INTO `%s` (`type`, `char_id`, `str`, `value`) VALUES", reg_db);
		count = 0;
		for(i=0;i<p->global_reg_num;i++)
		{
			if (p->global_reg[i].str && p->global_reg[i].value)
			{
				tmp_ptr += sprintf(tmp_ptr,"('3','%d','%s','%s'),",
					char_id, jstrescapecpy(temp_str,p->global_reg[i].str), jstrescapecpy(temp_str2,p->global_reg[i].value));
				if (++count%100 == 0)
				{ //Save every X registers to avoid overflowing tmp_sql [Skotlex]
					tmp_ptr[-1] = '\0';
					if(mysql_query(&mysql_handle, tmp_sql))
					{
						ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
						ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
					} else
						strcat(save_status, " global_reg");

					tmp_ptr = tmp_sql;
					tmp_ptr += sprintf(tmp_ptr,"INSERT INTO `%s` (`type`, `char_id`, `str`, `value`) VALUES", reg_db);
					count = 0;
				}
			}
		}

		if (count)
		{
			tmp_ptr[-1] = '\0';
			if(mysql_query(&mysql_handle, tmp_sql))
			{
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			} else
				strcat(save_status, " global_reg");
		} else //Values cleared.
			strcat(save_status, " global_reg");
	}

	diff = 0;
	for(i = 0; i < MAX_FRIENDS; i++){
		if(p->friends[i].char_id != cp->friends[i].char_id ||
			p->friends[i].account_id != cp->friends[i].account_id){
			diff = 1;
			break;
		}
	}

	if(diff == 1)
	{	//Save friends
		sprintf(tmp_sql, "DELETE FROM `%s` WHERE `char_id`='%d'", friend_db, char_id);
		if(mysql_query(&mysql_handle, tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}

		tmp_ptr = tmp_sql;
		tmp_ptr += sprintf(tmp_ptr, "INSERT INTO `%s` (`char_id`, `friend_account`, `friend_id`) VALUES ", friend_db);
		count = 0;
		for(i = 0; i < MAX_FRIENDS; i++){
			if(p->friends[i].char_id > 0)
			{
				tmp_ptr += sprintf(tmp_ptr, "('%d','%d','%d'),", char_id, p->friends[i].account_id, p->friends[i].char_id);
				count++;
			}
		}
		if (count)
		{
			tmp_ptr[-1] = '\0';	//Remove the last comma. [Skotlex]
			if(mysql_query(&mysql_handle, tmp_sql))
			{
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			} else
				strcat(save_status, " friends");
		} else //Friend list cleared.
			strcat(save_status, " friends");

	}

	if (save_status[0]!='\0' && save_log)
		ShowInfo("Saved char %d - %s:%s.\n", char_id, char_dat[0].name, save_status);
	memcpy(cp, p, sizeof(struct mmo_charstatus));

	return 0;
}

// [Ilpalazzo-sama]
int memitemdata_to_sql(struct itemtmp mapitem[], int count, int char_id, int tableswitch)
{
	int i,j, flag, id;
	char *tablename;
	char selectoption[16];
	char * str_p = tmp_sql;

	switch (tableswitch) {
	case TABLE_INVENTORY:
		tablename = inventory_db; // no need for sprintf here as *_db are char*.
		sprintf(selectoption,"char_id");
		break;
	case TABLE_CART:
		tablename = cart_db;
		sprintf(selectoption,"char_id");
		break;
	case TABLE_STORAGE:
		tablename = storage_db;
		sprintf(selectoption,"account_id");
		break;
	case TABLE_GUILD_STORAGE:
		tablename = guild_storage_db;
		sprintf(selectoption,"guild_id");
		break;
	default:
		ShowError("Invalid table name!\n");
		return 1;
	}

	//=======================================mysql database data > memory===============================================

	str_p += sprintf(str_p, "SELECT `id`, `nameid`, `amount`, `equip`, `identify`, `refine`, `attribute`");

	for (j=0; j<MAX_SLOTS; j++)
		str_p += sprintf(str_p, ", `card%d`", j);

	str_p += sprintf(str_p, " FROM `%s` WHERE `%s`='%d'", tablename, selectoption, char_id);

	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		return 1;
	}
	sql_res = mysql_store_result(&mysql_handle);
	if (sql_res) {
		while ((sql_row = mysql_fetch_row(sql_res))) {
			flag = 0;
			id = atoi(sql_row[0]);
			for(i = 0; i < count; i++) {
				if(mapitem[i].flag == 1)
					continue;
				if(mapitem[i].nameid == atoi(sql_row[1])
					&& mapitem[i].card[0] == atoi(sql_row[7])
					&& mapitem[i].card[2] == atoi(sql_row[9])
					&& mapitem[i].card[3] == atoi(sql_row[10])
				) {	//They are the same item.
					for (j = 0; j<MAX_SLOTS && mapitem[i].card[j] == atoi(sql_row[7+j]); j++);
					if (j == MAX_SLOTS &&
						mapitem[i].amount == atoi(sql_row[2]) &&
						mapitem[i].equip == atoi(sql_row[3]) &&
						mapitem[i].identify == atoi(sql_row[4]) &&
						mapitem[i].refine == atoi(sql_row[5]) &&
						mapitem[i].attribute == atoi(sql_row[6]))
					{ //Do nothing.
					} else
//==============================================Memory data > SQL ===============================
					if(!itemdb_isequip(mapitem[i].nameid))
					{	//Quick update of stackable items. Update Qty and Equip should be enough, but in case we are also updating identify
						sprintf(tmp_sql,"UPDATE `%s` SET `equip`='%d', `identify`='%d', `amount`='%d' WHERE `id`='%d' LIMIT 1",
							tablename, mapitem[i].equip, mapitem[i].identify,mapitem[i].amount, id);
						if(mysql_query(&mysql_handle, tmp_sql))
						{
							ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
							ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
						}
					} else
					{	//Equipment or Misc item, just update all fields.
						str_p = tmp_sql;
						str_p += sprintf(str_p,"UPDATE `%s` SET `equip`='%d', `identify`='%d', `refine`='%d',`attribute`='%d'",
							tablename, mapitem[i].equip, mapitem[i].identify, mapitem[i].refine, mapitem[i].attribute);

						for(j=0; j<MAX_SLOTS; j++)
							str_p += sprintf(str_p, ", `card%d`=%d", j, mapitem[i].card[j]);

						str_p += sprintf(str_p,", `amount`='%d' WHERE `id`='%d' LIMIT 1",
							mapitem[i].amount, id);

						if(mysql_query(&mysql_handle, tmp_sql))
						{
							ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
							ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
						}
					}
					flag = mapitem[i].flag = 1; //Item dealt with,
					break; //skip to next item in the db.
				}
			}
			if(!flag) { //Item not updated, remove it.
				sprintf(tmp_sql,"DELETE from `%s` where `id`='%d'", tablename, id);
				if(mysql_query(&mysql_handle, tmp_sql))
				{
					ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
					ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
				}
			}
		}
		mysql_free_result(sql_res);
	}

	for(i = 0; i < count; i++) {
		if(!mapitem[i].flag) {
			str_p = tmp_sql;
			str_p += sprintf(str_p,"INSERT INTO `%s`(`%s`, `nameid`, `amount`, `equip`, `identify`, `refine`, `attribute`",
				tablename, selectoption);
			for(j=0; j<MAX_SLOTS; j++)
				str_p += sprintf(str_p,", `card%d`", j);

			str_p += sprintf(str_p,") VALUES ( '%d','%d', '%d', '%d', '%d', '%d', '%d'",
				char_id, mapitem[i].nameid, mapitem[i].amount, mapitem[i].equip, mapitem[i].identify, mapitem[i].refine,
				mapitem[i].attribute);

			for(j=0; j<MAX_SLOTS; j++)
				str_p +=sprintf(str_p,", '%d'",mapitem[i].card[j]);

			strcat(tmp_sql, ")");
			if(mysql_query(&mysql_handle, tmp_sql))
			{
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
		}
	}
	return 0;
}

//=====================================================================================================
int mmo_char_fromsql(int char_id, struct mmo_charstatus *p){
	int i,j, n;
	int friends;
	char t_msg[128];
	char *str_p = tmp_sql;
	struct mmo_charstatus *cp;
        friends = 0;

	cp = (struct mmo_charstatus*)numdb_search(char_db_,char_id);
	if (cp != NULL)
	  aFree(cp);

	memset(p, 0, sizeof(struct mmo_charstatus));
	t_msg[0]= '\0';

	p->char_id = char_id;
	if (save_log)
		ShowInfo("Char load request (%d)\n", char_id);
	//`char`( `char_id`,`account_id`,`char_num`,`name`,`class`,`base_level`,`job_level`,`base_exp`,`job_exp`,`zeny`, //9
	//`str`,`agi`,`vit`,`int`,`dex`,`luk`, //15
	//`max_hp`,`hp`,`max_sp`,`sp`,`status_point`,`skill_point`, //21
	//`option`,`karma`,`manner`,`party_id`,`guild_id`,`pet_id`, //27
	//`hair`,`hair_color`,`clothes_color`,`weapon`,`shield`,`head_top`,`head_mid`,`head_bottom`, //35
	//`last_map`,`last_x`,`last_y`,`save_map`,`save_x`,`save_y`)
	//splite 2 parts. cause veeeery long SQL syntax

	sprintf(tmp_sql, "SELECT `char_id`,`account_id`,`char_num`,`name`,`class`,`base_level`,`job_level`,`base_exp`,`job_exp`,`zeny`,"
		"`str`,`agi`,`vit`,`int`,`dex`,`luk`, `max_hp`,`hp`,`max_sp`,`sp`,`status_point`,`skill_point` FROM `%s` WHERE `char_id` = '%d'",char_db, char_id); // TBR

	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	sql_res = mysql_store_result(&mysql_handle);

	if (sql_res) {
		sql_row = mysql_fetch_row(sql_res);
		if (!sql_row)
		{	//Just how does this happens? [Skotlex]
			ShowError("Requested non-existant character id: %d!\n", char_id);
			numdb_erase(char_db_,char_id);
			return 0;
		}

		p->char_id = char_id;
		p->account_id = atoi(sql_row[1]);
		p->char_num = atoi(sql_row[2]);
		strcpy(p->name, sql_row[3]);
		p->class_ = atoi(sql_row[4]);
		p->base_level = atoi(sql_row[5]);
		p->job_level = atoi(sql_row[6]);
		p->base_exp = atoi(sql_row[7]);
		p->job_exp = atoi(sql_row[8]);
		p->zeny = atoi(sql_row[9]);
		p->str = atoi(sql_row[10]);
		p->agi = atoi(sql_row[11]);
		p->vit = atoi(sql_row[12]);
		p->int_ = atoi(sql_row[13]);
		p->dex = atoi(sql_row[14]);
		p->luk = atoi(sql_row[15]);
		p->max_hp = atoi(sql_row[16]);
		p->hp = atoi(sql_row[17]);
		p->max_sp = atoi(sql_row[18]);
		p->sp = atoi(sql_row[19]);
		p->status_point = atoi(sql_row[20]);
		p->skill_point = atoi(sql_row[21]);
		//free mysql result.
		mysql_free_result(sql_res);
		strcat (t_msg, " status");
	} else
		ShowError("Load char failed (%d - table %s).\n", char_id, char_db);	//Error?! ERRRRRR WHAT THAT SAY!?

	sprintf(tmp_sql, "SELECT `option`,`karma`,`manner`,`party_id`,`guild_id`,`pet_id`,`hair`,`hair_color`,"
		"`clothes_color`,`weapon`,`shield`,`head_top`,`head_mid`,`head_bottom`,"
		"`last_map`,`last_x`,`last_y`,`save_map`,`save_x`,`save_y`, `partner_id`, `father`, `mother`, `child`, `fame`"
		"FROM `%s` WHERE `char_id` = '%d'",char_db, char_id); // TBR
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	sql_res = mysql_store_result(&mysql_handle);
	sql_row = sql_res?mysql_fetch_row(sql_res):NULL;
	if (sql_row) {

		p->option = atoi(sql_row[0]);	p->karma = atoi(sql_row[1]);	p->manner = atoi(sql_row[2]);
			p->party_id = atoi(sql_row[3]);	p->guild_id = atoi(sql_row[4]);	p->pet_id = atoi(sql_row[5]);

		p->hair = atoi(sql_row[6]);	p->hair_color = atoi(sql_row[7]);	p->clothes_color = atoi(sql_row[8]);
		p->weapon = atoi(sql_row[9]);	p->shield = atoi(sql_row[10]);
		p->head_top = atoi(sql_row[11]);	p->head_mid = atoi(sql_row[12]);	p->head_bottom = atoi(sql_row[13]);
		strcpy(p->last_point.map,sql_row[14]); p->last_point.x = atoi(sql_row[15]);	p->last_point.y = atoi(sql_row[16]);
		strcpy(p->save_point.map,sql_row[17]); p->save_point.x = atoi(sql_row[18]);	p->save_point.y = atoi(sql_row[19]);
		p->partner_id = atoi(sql_row[20]); p->father = atoi(sql_row[21]); p->mother = atoi(sql_row[22]); p->child = atoi(sql_row[23]);
		p->fame = atoi(sql_row[24]);

		//free mysql result.
		mysql_free_result(sql_res);
		strcat (t_msg, " status2");
	} else
		ShowError("Char load failed (%d - table %s)\n", char_id, char_db);	//Error?! ERRRRRR WHAT THAT SAY!?

	if (p->last_point.x == 0 || p->last_point.y == 0 || p->last_point.map[0] == '\0')
		memcpy(&p->last_point, &start_point, sizeof(start_point));

	if (p->save_point.x == 0 || p->save_point.y == 0 || p->save_point.map[0] == '\0')
		memcpy(&p->save_point, &start_point, sizeof(start_point));

	//read memo data
	//`memo` (`memo_id`,`char_id`,`map`,`x`,`y`)
	sprintf(tmp_sql, "SELECT `map`,`x`,`y` FROM `%s` WHERE `char_id`='%d' ORDER by `memo_id`",memo_db, char_id); // TBR
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	sql_res = mysql_store_result(&mysql_handle);

	if (sql_res) {
		for(i=0;(sql_row = mysql_fetch_row(sql_res));i++){
			strcpy (p->memo_point[i].map,sql_row[0]);
			p->memo_point[i].x=atoi(sql_row[1]);
			p->memo_point[i].y=atoi(sql_row[2]);
			//i ++;
		}
		mysql_free_result(sql_res);
		strcat (t_msg, " memo");
	}

	//read inventory
	//`inventory` (`id`,`char_id`, `nameid`, `amount`, `equip`, `identify`, `refine`, `attribute`, `card0`, `card1`, `card2`, `card3`)
	str_p += sprintf(str_p, "SELECT `id`, `nameid`, `amount`, `equip`, `identify`, `refine`, `attribute`");

	for (j=0; j<MAX_SLOTS; j++)
		str_p += sprintf(str_p, ", `card%d`", j);

	str_p += sprintf(str_p, " FROM `%s` WHERE `char_id`='%d'", inventory_db, char_id);

	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	sql_res = mysql_store_result(&mysql_handle);
	if (sql_res) {
		for(i=0;(sql_row = mysql_fetch_row(sql_res));i++){
			p->inventory[i].id = atoi(sql_row[0]);
			p->inventory[i].nameid = atoi(sql_row[1]);
			p->inventory[i].amount = atoi(sql_row[2]);
			p->inventory[i].equip = atoi(sql_row[3]);
			p->inventory[i].identify = atoi(sql_row[4]);
			p->inventory[i].refine = atoi(sql_row[5]);
			p->inventory[i].attribute = atoi(sql_row[6]);
			for (j=0; j<MAX_SLOTS; j++)
				p->inventory[i].card[j] = atoi(sql_row[7+j]);
		}
		mysql_free_result(sql_res);
		strcat (t_msg, " inventory");
	}

	//read cart.
	//`cart_inventory` (`id`,`char_id`, `nameid`, `amount`, `equip`, `identify`, `refine`, `attribute`, `card0`, `card1`, `card2`, `card3`)
	str_p = tmp_sql;
	str_p += sprintf(str_p, "SELECT `id`, `nameid`, `amount`, `equip`, `identify`, `refine`, `attribute`");

	for (j=0; j<MAX_SLOTS; j++)
		str_p += sprintf(str_p, ", `card%d`", j);

	str_p += sprintf(str_p, " FROM `%s` WHERE `char_id`='%d'", cart_db, char_id);

	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	sql_res = mysql_store_result(&mysql_handle);
	if (sql_res) {
		for(i=0;(sql_row = mysql_fetch_row(sql_res));i++){
		        p->cart[i].id = atoi(sql_row[0]);
			p->cart[i].nameid = atoi(sql_row[1]);
			p->cart[i].amount = atoi(sql_row[2]);
			p->cart[i].equip = atoi(sql_row[3]);
			p->cart[i].identify = atoi(sql_row[4]);
			p->cart[i].refine = atoi(sql_row[5]);
			p->cart[i].attribute = atoi(sql_row[6]);
			for(j=0; j<MAX_SLOTS; j++)
				p->cart[i].card[j] = atoi(sql_row[7+j]);
		}
		mysql_free_result(sql_res);
		strcat (t_msg, " cart");
	}

	//read skill
	//`skill` (`char_id`, `id`, `lv`)
	sprintf(tmp_sql, "SELECT `id`, `lv` FROM `%s` WHERE `char_id`='%d'",skill_db, char_id); // TBR
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	sql_res = mysql_store_result(&mysql_handle);
	if (sql_res) {
		for(i=0;(sql_row = mysql_fetch_row(sql_res));i++){
			n = atoi(sql_row[0]);
			p->skill[n].id = n; //memory!? shit!.
			p->skill[n].lv = atoi(sql_row[1]);
		}
		mysql_free_result(sql_res);
		strcat (t_msg, " skills");
	}

	//global_reg
	//`global_reg_value` (`char_id`, `str`, `value`)
	sprintf(tmp_sql, "SELECT `str`, `value` FROM `%s` WHERE `type`=3 AND `char_id`='%d'",reg_db, char_id); // TBR
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	i = 0;
	sql_res = mysql_store_result(&mysql_handle);
	if (sql_res) {
		for(i=0;(sql_row = mysql_fetch_row(sql_res));i++){
			strcpy (p->global_reg[i].str, sql_row[0]);
			strcpy (p->global_reg[i].value, sql_row[1]);
		}
		mysql_free_result(sql_res);
		strcat (t_msg, " reg_values");
	}
	p->global_reg_num=i;

	//Shamelessly stolen from its_sparky (ie: thanks) and then assimilated by [Skotlex]
	//Friend list
	sprintf(tmp_sql, "SELECT f.friend_account, f.friend_id, c.name FROM `%s` f LEFT JOIN `%s` c ON f.friend_account=c.account_id AND f.friend_id=c.char_id WHERE f.char_id='%d'", friend_db, char_db, char_id);

	if(mysql_query(&mysql_handle, tmp_sql)){
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	sql_res = mysql_store_result(&mysql_handle);
	if(sql_res)
	{
		for(i = 0; (sql_row = mysql_fetch_row(sql_res)) && i<MAX_FRIENDS; i++)
		{
			if(sql_row) { //need to check if we have sql_row before we check if we have sql_row[2] because we don't want a segfault
				if(sql_row[2]) {
					p->friends[i].account_id = atoi(sql_row[0]);
					p->friends[i].char_id = atoi(sql_row[1]);
					strncpy(p->friends[i].name, sql_row[2], NAME_LENGTH-1); //The -1 is to avoid losing the ending \0 [Skotlex]
				}
			}
		}
		mysql_free_result(sql_res);
		strcat (t_msg, " friends");
	}

	if (save_log)
		ShowInfo("Loaded char (%d - %s): %s\n", char_id, p->name, t_msg);	//ok. all data load successfuly!

	cp = (struct mmo_charstatus *) aMalloc(sizeof(struct mmo_charstatus));
    	memcpy(cp, p, sizeof(struct mmo_charstatus));
	numdb_insert(char_db_, char_id,cp);

	return 1;
}

// For quick selection of data when displaying the char menu. [Skotlex]
//
int mmo_char_fromsql_short(int char_id, struct mmo_charstatus *p){
	char t_msg[128];

	/* The quick method does not loads a complete character, so we bypass the db.
	cp = (struct mmo_charstatus*)numdb_search(char_db_,char_id);
	if (cp != NULL)
	  aFree(cp);
	*/
	memset(p, 0, sizeof(struct mmo_charstatus));
	t_msg[0]= '\0';

	p->char_id = char_id;
//	ShowInfo("Quick Char load request (%d)\n", char_id);
	//`char`( `char_id`,`account_id`,`char_num`,`name`,`class`,`base_level`,`job_level`,`base_exp`,`job_exp`,`zeny`, //9
	//`str`,`agi`,`vit`,`int`,`dex`,`luk`, //15
	//`max_hp`,`hp`,`max_sp`,`sp`,`status_point`,`skill_point`, //21
	//`option`,`karma`,`manner`,`party_id`,`guild_id`,`pet_id`, //27
	//`hair`,`hair_color`,`clothes_color`,`weapon`,`shield`,`head_top`,`head_mid`,`head_bottom`, //35
	//`last_map`,`last_x`,`last_y`,`save_map`,`save_x`,`save_y`)
	//splite 2 parts. cause veeeery long SQL syntax

	sprintf(tmp_sql, "SELECT `char_id`,`account_id`,`char_num`,`name`,`class`,`base_level`,`job_level`,`base_exp`,`job_exp`,`zeny`,"
		"`str`,`agi`,`vit`,`int`,`dex`,`luk`, `max_hp`,`hp`,`max_sp`,`sp`,`status_point`,`skill_point` FROM `%s` WHERE `char_id` = '%d'",char_db, char_id); // TBR

	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	sql_res = mysql_store_result(&mysql_handle);

	if (sql_res) {
		sql_row = mysql_fetch_row(sql_res);
		if (!sql_row)
		{	//Just how does this happens? [Skotlex]
			ShowError("Requested non-existant character id: %d!\n", char_id);
			return 0;
		}
		p->char_id = char_id;
		p->account_id = atoi(sql_row[1]);
		p->char_num = atoi(sql_row[2]);
		strcpy(p->name, sql_row[3]);
		p->class_ = atoi(sql_row[4]);
		p->base_level = atoi(sql_row[5]);
		p->job_level = atoi(sql_row[6]);
		p->base_exp = atoi(sql_row[7]);
		p->job_exp = atoi(sql_row[8]);
		p->zeny = atoi(sql_row[9]);
		p->str = atoi(sql_row[10]);
		p->agi = atoi(sql_row[11]);
		p->vit = atoi(sql_row[12]);
		p->int_ = atoi(sql_row[13]);
		p->dex = atoi(sql_row[14]);
		p->luk = atoi(sql_row[15]);
		p->max_hp = atoi(sql_row[16]);
		p->hp = atoi(sql_row[17]);
		p->max_sp = atoi(sql_row[18]);
		p->sp = atoi(sql_row[19]);
		p->status_point = atoi(sql_row[20]);
		p->skill_point = atoi(sql_row[21]);
		//free mysql result.
		mysql_free_result(sql_res);
		strcat (t_msg, " status");
	} else
		ShowError("Load char failed (%d - table %s).\n", char_id, char_db);	//Error?! ERRRRRR WHAT THAT SAY!?

	sprintf(tmp_sql, "SELECT `option`,`karma`,`manner`,`hair`,`hair_color`,"
		"`clothes_color`,`weapon`,`shield`,`head_top`,`head_mid`,`head_bottom`"
		"FROM `%s` WHERE `char_id` = '%d'",char_db, char_id); // TBR
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	sql_res = mysql_store_result(&mysql_handle);
	sql_row = sql_res?mysql_fetch_row(sql_res):NULL;
	if (sql_row) {

		p->option = atoi(sql_row[0]);	p->karma = atoi(sql_row[1]);	p->manner = atoi(sql_row[2]);
		p->hair = atoi(sql_row[3]);	p->hair_color = atoi(sql_row[4]);	p->clothes_color = atoi(sql_row[5]);
		p->weapon = atoi(sql_row[6]);	p->shield = atoi(sql_row[7]);
		p->head_top = atoi(sql_row[8]);	p->head_mid = atoi(sql_row[9]);	p->head_bottom = atoi(sql_row[10]);

		//free mysql result.
		mysql_free_result(sql_res);
		strcat (t_msg, " status2");
	} else
		ShowError("Char load failed (%d - table %s)\n", char_id, char_db);	//Error?! ERRRRRR WHAT THAT SAY!?

	if (save_log)
		ShowInfo("Quick Loaded char (%d - %s): %s\n", char_id, p->name, t_msg);	//ok. all data load successfuly!

//	cp = (struct mmo_charstatus *) aMalloc(sizeof(struct mmo_charstatus)); //The loaded char is temporary, so no need for long-term storage [Skotlex]
//		memcpy(cp, p, sizeof(struct mmo_charstatus));

	//numdb_insert(char_db_, char_id,cp);

	return 1;
}
//==========================================================================================================
int mmo_char_sql_init(void) {
	int charcount;

        char_db_=numdb_init();

	ShowInfo("Begin Initializing.......\n");
	// memory initialize
	// no need to set twice size in this routine. but some cause segmentation error. :P
	// The hell? Why segmentation faults? Sounds like a bug that needs addressing... [Skotlex]
	ShowDebug("initializing char memory...(%d byte)\n",sizeof(struct mmo_charstatus)*2);
	CREATE(char_dat, struct mmo_charstatus, 2);

	memset(char_dat, 0, sizeof(struct mmo_charstatus)*2);

	//Check for max id (in case new chars would get their IDs set below 150K) [Skotlex]
	sprintf(tmp_sql , "SELECT max(`char_id`) FROM `%s`", char_db);
	if (mysql_query(&mysql_handle, tmp_sql)) {
		printf("DB server Error (Select max char_id) - %s\n", mysql_error(&mysql_handle));
	} else {
		sql_res = mysql_store_result(&mysql_handle);
		if (sql_res)
		{
			if (mysql_num_rows(sql_res) > 0 &&
				(sql_row = mysql_fetch_row(sql_res)) != NULL &&
				sql_row[0] != NULL && atoi(sql_row[0]) >= char_id_count)
				char_id_count = 0;	//No need for setting the char id.
			mysql_free_result(sql_res);
		}
	}

	sprintf(tmp_sql, "SELECT `char_id` FROM `%s`", char_db);
	if(mysql_query(&mysql_handle, tmp_sql)){
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}else{
		sql_res = mysql_store_result(&mysql_handle);
		if(sql_res){
			charcount = (int)mysql_num_rows(sql_res);
			ShowStatus("total char data -> '%d'.......\n", charcount);
			mysql_free_result(sql_res);
		}else{
			ShowStatus("total char data -> '0'.......\n");
		}
	}

	if(char_per_account == 0){
	  ShowStatus("Chars per Account: 'Unlimited'.......\n");
        }else{
          ShowStatus("Chars per Account: '%d'.......\n", char_per_account);
        }

	//the 'set offline' part is now in check_login_conn ...
	//if the server connects to loginserver
	//it will dc all off players
	//and send the loginserver the new state....

	ShowInfo("Finished initilizing.......\n");

	return 0;
}

//==========================================================================================================

int make_new_char_sql(int fd, unsigned char *dat) {
	struct char_session_data *sd;
	char t_name[NAME_LENGTH*2];
	unsigned int i; // Used in for loop and comparing with strlen, safe to be unsigned. [Lance]
	int char_id, temp;

	//Check for char name length overflows [Skotlex]
	if (strlen((char *)dat) > NAME_LENGTH-1)
		dat[NAME_LENGTH-1] = '\0';

	dat = (unsigned char*)trim((char *)dat,TRIM_CHARS); //Trim character name. [Skotlex]
	jstrescapecpy(t_name, (char*)dat);

	// disabled until fixed >.>
	// Note: escape characters should be added to jstrescape()!
	//mysql_real_escape_string(&mysql_handle, t_name, t_name_temp, sizeof(t_name_temp));

	if (!session_isValid(fd) || !(sd = (struct char_session_data*)session[fd]->session_data))
		return -2;

	ShowInfo("New character request (%d)\n", sd->account_id);

	//check for charcount (maxchars) :)
	if(char_per_account != 0){
		sprintf(tmp_sql, "SELECT `account_id` FROM `%s` WHERE `account_id` = '%d'", char_db, sd->account_id);
		if(mysql_query(&mysql_handle, tmp_sql)){
			ShowError("fail, SQL Error: %s !!FAIL!!\n", tmp_sql);
		}
		sql_res = mysql_store_result(&mysql_handle);
		if(sql_res){
			//ok
			temp = (int)mysql_num_rows(sql_res);
			if(temp >= char_per_account){
				//hehe .. limit exceeded :P
				ShowInfo("Create char failed (%d): charlimit exceeded.\n", sd->account_id);
				mysql_free_result(sql_res);
				return -2;
			}
			mysql_free_result(sql_res);
		}
	}

	// Check Authorised letters/symbols in the name of the character
	if (char_name_option == 1) { // only letters/symbols in char_name_letters are authorised
		for (i = 0; i < strlen((const char*)dat); i++)
			if (strchr(char_name_letters, dat[i]) == NULL)
				return -2;
	} else if (char_name_option == 2) { // letters/symbols in char_name_letters are forbidden
		for (i = 0; i < strlen((const char*)dat); i++)
			if (strchr(char_name_letters, dat[i]) != NULL)
				return -2;
	} // else, all letters/symbols are authorised (except control char removed before)

	//check stat error
	if ((dat[24]+dat[25]+dat[26]+dat[27]+dat[28]+dat[29]!=6*5 ) || // stats
		(dat[30] >= 9) || // slots (dat[30] can not be negativ)
		(dat[33] <= 0) || (dat[33] >= 24) || // hair style
		(dat[31] >= 9)) { // hair color (dat[31] can not be negativ)
		if (log_char) {
			// char.log to charlog
			sprintf(tmp_sql,"INSERT INTO `%s` (`time`, `char_msg`,`account_id`,`char_num`,`name`,`str`,`agi`,`vit`,`int`,`dex`,`luk`,`hair`,`hair_color`)"
				"VALUES (NOW(), '%s', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d')",
				charlog_db,"make new char error", sd->account_id, dat[30], dat, dat[24], dat[25], dat[26], dat[27], dat[28], dat[29], dat[33], dat[31]);
			//query
			mysql_query(&mysql_handle, tmp_sql);
		}
		ShowWarning("Create char failed (%d): stats error (bot cheat?!)\n", sd->account_id);
		return -2;
	} // for now we have checked: stat points used <31, char slot is less then 9, hair style/color values are acceptable

	// check individual stat value
	for(i = 24; i <= 29; i++) {
		if (dat[i] < 1 || dat[i] > 9) {
			if (log_char) {
				// char.log to charlog
				sprintf(tmp_sql,"INSERT INTO `%s` (`time`, `char_msg`,`account_id`,`char_num`,`name`,`str`,`agi`,`vit`,`int`,`dex`,`luk`,`hair`,`hair_color`)"
					"VALUES (NOW(), '%s', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d')",
					charlog_db,"make new char error", sd->account_id, dat[30], dat, dat[24], dat[25], dat[26], dat[27], dat[28], dat[29], dat[33], dat[31]);
				//query
				mysql_query(&mysql_handle, tmp_sql);
			}
			ShowWarning("Create char failed (%d): stats error (bot cheat?!)\n", sd->account_id);
				return -2;
		}
	} // now we know that every stat has proper value but we have to check if str/int agi/luk vit/dex pairs are correct

	if( ((dat[24]+dat[27]) > 10) || ((dat[25]+dat[29]) > 10) || ((dat[26]+dat[28]) > 10) ) {
		if (log_char) {
			// char.log to charlog
			sprintf(tmp_sql,"INSERT INTO `%s` (`time`, `char_msg`,`account_id`,`char_num`,`name`,`str`,`agi`,`vit`,`int`,`dex`,`luk`,`hair`,`hair_color`)"
					"VALUES (NOW(), '%s', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d')",
					charlog_db,"make new char error", sd->account_id, dat[30], dat, dat[24], dat[25], dat[26], dat[27], dat[28], dat[29], dat[33], dat[31]);
			//query
			mysql_query(&mysql_handle, tmp_sql);
		}
		ShowWarning("Create char failed (%d): stats error (bot cheat?!)\n", sd->account_id);
		return -2;
	} // now when we have passed all stat checks

	if (log_char) {
		// char.log to charlog
		sprintf(tmp_sql,"INSERT INTO `%s`(`time`, `char_msg`,`account_id`,`char_num`,`name`,`str`,`agi`,`vit`,`int`,`dex`,`luk`,`hair`,`hair_color`)"
			"VALUES (NOW(), '%s', '%d', '%d', '%s', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d')",
			charlog_db,"make new char", sd->account_id, dat[30], dat, dat[24], dat[25], dat[26], dat[27], dat[28], dat[29], dat[33], dat[31]);
		//query
		if (mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
	}
	//printf("make new char %d-%d %s %d, %d, %d, %d, %d, %d - %d, %d" RETCODE,
	//	fd, dat[30], dat, dat[24], dat[25], dat[26], dat[27], dat[28], dat[29], dat[33], dat[31]);

	//Check Name (already in use?)
	sprintf(tmp_sql, "SELECT `name` FROM `%s` WHERE `name` = '%s'",char_db, t_name);
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		return -2;
	}
	sql_res = mysql_store_result(&mysql_handle);
	if(sql_res){
		temp = (int)mysql_num_rows(sql_res);

		if (temp > 0) {
			mysql_free_result(sql_res);
			ShowInfo("Create char failed: charname already in use\n");
			return -1;
		}
		mysql_free_result(sql_res);
	}

	// check char slot.
	sprintf(tmp_sql, "SELECT `account_id`, `char_num` FROM `%s` WHERE `account_id` = '%d' AND `char_num` = '%d'",char_db, sd->account_id, dat[30]);
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	sql_res = mysql_store_result(&mysql_handle);

	if(sql_res){
		temp = (int)mysql_num_rows(sql_res);

		if (temp > 0) {
			mysql_free_result(sql_res);
			ShowWarning("Create char failed (%d, slot: %d), slot already in use\n", sd->account_id, dat[30]);
			return -2;
		}
		mysql_free_result(sql_res);
	}

	//New Querys [Sirius]
	//Insert the char to the 'chardb' ^^
	if (char_id_count) //Force initial char id. [Skotlex]
		sprintf(tmp_sql, "INSERT INTO `%s` (`char_id`,`account_id`, `char_num`, `name`, `zeny`, `str`, `agi`, `vit`, `int`, `dex`, `luk`, `max_hp`, `hp`, `max_sp`, `sp`, `hair`, `hair_color`, `last_map`, `last_x`, `last_y`, `save_map`, `save_x`, `save_y`) VALUES ('%d', '%d', '%d', '%s', '%d',  '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d','%d', '%d','%d', '%d', '%s', '%d', '%d', '%s', '%d', '%d')", char_db, char_id_count, sd->account_id , dat[30] , t_name, start_zeny, dat[24], dat[25], dat[26], dat[27], dat[28], dat[29], (40 * (100 + dat[26])/100) , (40 * (100 + dat[26])/100 ),  (11 * (100 + dat[27])/100), (11 * (100 + dat[27])/100), dat[33], dat[31], start_point.map, start_point.x, start_point.y, start_point.map, start_point.x, start_point.y);
	else
		sprintf(tmp_sql, "INSERT INTO `%s` (`account_id`, `char_num`, `name`, `zeny`, `str`, `agi`, `vit`, `int`, `dex`, `luk`, `max_hp`, `hp`, `max_sp`, `sp`, `hair`, `hair_color`, `last_map`, `last_x`, `last_y`, `save_map`, `save_x`, `save_y`) VALUES ('%d', '%d', '%s', '%d',  '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d','%d', '%d','%d', '%d', '%s', '%d', '%d', '%s', '%d', '%d')", char_db, sd->account_id , dat[30] , t_name, start_zeny, dat[24], dat[25], dat[26], dat[27], dat[28], dat[29], (40 * (100 + dat[26])/100) , (40 * (100 + dat[26])/100 ),  (11 * (100 + dat[27])/100), (11 * (100 + dat[27])/100), dat[33], dat[31], start_point.map, start_point.x, start_point.y, start_point.map, start_point.x, start_point.y);
	if(mysql_query(&mysql_handle, tmp_sql)){
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		return -2; //No, stop the procedure!
	}

	if (char_id_count) //Clear this out for future inserts.
		char_id_count = 0;

	//Now we need the charid from sql!
	sprintf(tmp_sql, "SELECT `char_id` FROM `%s` WHERE `account_id` = '%d' AND `char_num` = '%d' AND `name` = '%s'", char_db, sd->account_id , dat[30] , t_name);
	if(mysql_query(&mysql_handle, tmp_sql)){
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		//delete the char ..(no trash in DB!)
		sprintf(tmp_sql, "DELETE FROM `%s` WHERE `account_id` = '%d' AND `char_num` = '%d' AND `name` = '%s'", char_db, sd->account_id, dat[30], t_name);
		mysql_query(&mysql_handle, tmp_sql);
		return -2; //XD end of the (World? :P) .. charcreate (denied)
	} else {
		//query ok -> get the data!
		sql_res = mysql_store_result(&mysql_handle);
		if(sql_res){
			sql_row = mysql_fetch_row(sql_res);
			char_id = sql_row?atoi(sql_row[0]):0; //char id :)
			mysql_free_result(sql_res);
			if(char_id <= 0){
				ShowError("failed (get char id..) CHARID (%d) wrong!\n", char_id);
				sprintf(tmp_sql, "DELETE FROM `%s` WHERE `account_id` = '%d' AND `char_num` = '%d' AND `name` = '%s'", char_db, sd->account_id, dat[30], t_name);
				mysql_query(&mysql_handle, tmp_sql);
				return -2; //charcreate denied ..
			}
		}else{
			//prevent to crash (if its false, and we want to free -> segfault :)
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			sprintf(tmp_sql, "DELETE FROM `%s` WHERE `account_id` = '%d' AND `char_num` = '%d' AND `name` = '%s'", char_db, sd->account_id, dat[30], t_name);
			mysql_query(&mysql_handle, tmp_sql);
			return -2; //end ...... -> charcreate failed :)
		}
	}

	//Give the char the default items
	//`inventory` (`id`,`char_id`, `nameid`, `amount`, `equip`, `identify`, `refine`, `attribute`, `card0`, `card1`, `card2`, `card3`)
	if (start_weapon > 0) { //add Start Weapon (Knife?)
		sprintf(tmp_sql,"INSERT INTO `%s` (`char_id`,`nameid`, `amount`, `equip`, `identify`) VALUES ('%d', '%d', '%d', '%d', '%d')", inventory_db, char_id, start_weapon,1,0x02,1);
		if (mysql_query(&mysql_handle, tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			sprintf(tmp_sql, "DELETE FROM `%s` WHERE `account_id` = '%d' AND `char_num` = '%d' AND `name` = '%s'", char_db, sd->account_id, dat[30], t_name);
			mysql_query(&mysql_handle, tmp_sql);
			return -2;//end XD
		}
	}
	if (start_armor > 0) { //Add default armor (cotton shirt?)
		sprintf(tmp_sql,"INSERT INTO `%s` (`char_id`,`nameid`, `amount`, `equip`, `identify`) VALUES ('%d', '%d', '%d', '%d', '%d')", inventory_db, char_id, start_armor,1,0x10,1);
		if (mysql_query(&mysql_handle, tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			sprintf(tmp_sql, "DELETE FROM `%s` WHERE `account_id` = '%d' AND `char_num` = '%d' AND `name` = '%s'", char_db, sd->account_id, dat[30], t_name);
			mysql_query(&mysql_handle, tmp_sql);
			sprintf(tmp_sql, "DELETE FROM `%s` WHERE `char_id` = '%d'", inventory_db, char_id);
			mysql_query(&mysql_handle, tmp_sql);
			return -2; //end....
		}
	}

         //DEPRECATED until the new friend table
         /*
         if(!insert_friends(char_id)){
		printf("fail (friendlist entrys..)\n");
			sprintf(tmp_sql, "DELETE FROM `%s` WHERE `char_id` = '%d'", char_db, char_id);
			mysql_query(&mysql_handle, tmp_sql);
			sprintf(tmp_sql, "DELETE FROM `%s` WHERE `char_id` = '%d'", inventory_db, char_id);
			mysql_query(&mysql_handle, tmp_sql);
			return -2; //end.. charcreate failed
	}
         */
	//printf("making new char success - id:(\033[1;32m%d\033[0m\tname:\033[1;32%s\033[0m\n", char_id, t_name);
	ShowInfo("Created char: account: %d, char: %d, slot: %d, name: %s\n", sd->account_id, char_id, dat[30], t_name);
	return char_id;
}

/*----------------------------------------------------------------------------------------------------------*/
/* Delete char - davidsiaw */
/*----------------------------------------------------------------------------------------------------------*/
/* Returns 0 if successful
 * Returns < 0 for error
 */
int delete_char_sql(int char_id, int partner_id)
{
	char char_name[NAME_LENGTH], t_name[NAME_LENGTH*2]; //Name needs be escaped.
	int account_id=0, party_id=0, guild_id=0;

	sprintf(tmp_sql, "SELECT `name`,`account_id`,`party_id`,`guild_id` FROM `%s` WHERE `char_id`='%d'",char_db, char_id);

	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	sql_res = mysql_store_result(&mysql_handle);

	if(sql_res)
		sql_row = mysql_fetch_row(sql_res);

	if (sql_res == NULL || sql_row == NULL)
	{
		ShowError("delete_char_sql: Unable to fetch character data, deletion aborted.\n");
		return -1;
	}
	strncpy(char_name, sql_row[0], NAME_LENGTH);
	char_name[NAME_LENGTH-1] = '\0';
	jstrescapecpy(t_name, char_name); //Escape string for sql use... [Skotlex]
	account_id = atoi(sql_row[1]);
	party_id = atoi(sql_row[2]);
	guild_id = atoi(sql_row[3]);
	mysql_free_result(sql_res); //Let's free this as soon as possible to avoid problems later on.

	/* Divorce [Wizputer] */
	if (partner_id) {
		sprintf(tmp_sql,"UPDATE `%s` SET `partner_id`='0' WHERE `char_id`='%d'",char_db,partner_id);
		if(mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
		sprintf(tmp_sql,"DELETE FROM `%s` WHERE (`nameid`='%d' OR `nameid`='%d') AND `char_id`='%d'",inventory_db,WEDDING_RING_M,WEDDING_RING_F,partner_id);
		if(mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
	}

	//Make the character leave the party [Skotlex]
	if (party_id)
		inter_party_leave(party_id, account_id);

	/* delete char's pet */
	//Delete the hatched pet if you have one...
	sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d' AND `incuvate` = '0'",pet_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	// Komurka's suggested way to clear pets, modified by [Skotlex] (because I always personalize what I do :X)
	//Removing pets that are in the char's inventory....
	{	//NOTE: The syntax for multi-table deletes is a bit changed between 4.0 and 4.1 regarding aliases, so we have to consider the version... [Skotlex]
		//Since we only care about the major and minor version, a double conversion is good enough. (4.1.20 -> 4.10000)
		double mysql_version = atof(mysql_get_server_info(&mysql_handle));

		sprintf(tmp_sql,
		"delete FROM `%s` USING `%s` as c LEFT JOIN `%s` as i ON c.char_id = i.char_id, `%s` as p WHERE c.char_id = '%d' AND i.card0 = -256 AND p.pet_id = (i.card1|(i.card2<<2))",
			(mysql_version<4.1?pet_db:"p"), char_db, inventory_db, pet_db, char_id);

		if(mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}

		//Removing pets that are in the char's cart....
		sprintf(tmp_sql,
		"delete FROM `%s` USING `%s` as c LEFT JOIN `%s` as i ON c.char_id = i.char_id, `%s` as p WHERE c.char_id = '%d' AND i.card0 = -256 AND p.pet_id = (i.card1|(i.card2<<2))",
			(mysql_version<4.1?pet_db:"p"), char_db, cart_db, pet_db, char_id);

		if(mysql_query(&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
	}

	/* delete char's friends list */
	sprintf(tmp_sql, "DELETE FROM `%s` WHERE `char_id` = '%d'",friend_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	/* delete char from other's friend list */
	//NOTE: Won't this cause problems for people who are already online? [Skotlex]
	sprintf(tmp_sql, "DELETE FROM `%s` WHERE `friend_id` = '%d'",friend_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	/* delete inventory */
	sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",inventory_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	/* delete cart inventory */
	sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",cart_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	/* delete memo areas */
	sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",memo_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	/* delete skills */
	sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",skill_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	/* delete character */
	sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",char_db, char_id);
	if(mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	/* No need as we used inter_guild_leave [Skotlex]
	// Also delete info from guildtables.
	sprintf(tmp_sql,"DELETE FROM `%s` WHERE `char_id`='%d'",guild_member_db, char_id);
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	*/

	sprintf(tmp_sql, "SELECT `guild_id` FROM `%s` WHERE `master` = '%s'", guild_db, t_name);

	if (mysql_query(&mysql_handle, tmp_sql))
	{
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	} else {
		sql_res = mysql_store_result(&mysql_handle);

		if (sql_res == NULL) {
			if (mysql_errno(&mysql_handle) != 0)
			{
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			return -1;
		} else {
			int rows = (int)mysql_num_rows(sql_res);
			mysql_free_result(sql_res);
			if (rows > 0) {
				mapif_parse_BreakGuild(0,guild_id);
			}
			else if (guild_id) //Leave your guild.
				inter_guild_leave(guild_id, account_id, char_id);
		}
	}
	return 0;
}

//==========================================================================================================

void mmo_char_sync(void)
{
	ShowWarning("mmo_char_sync() - nothing to do\n");
}

// to do
///////////////////////////

int mmo_char_sync_timer(int tid, unsigned int tick, int id, int data) {
	ShowWarning("mmo_char_sync_timer() tic - no works to do\n");
	return 0;
}

int count_users(void) {
	int i, users;

	if (login_fd > 0 && session[login_fd]){
		users = 0;
		for(i = 0; i < MAX_MAP_SERVERS; i++) {
			if (server_fd[i] > 0) {
				users += server[i].users;
			}
		}
		return users;
	}
	return 0;
}

int mmo_char_send006b(int fd, struct char_session_data *sd) {
	int i, j, found_num = 0;
	struct mmo_charstatus *p = NULL;
// hehe. commented other. anyway there's no need to use older version.
// if use older packet version just uncomment that!
//#ifdef NEW_006b
	const int offset = 24;
//#else
//	int offset = 4;
//#endif

//	ShowDebug("mmo_char_send006b start.. (account:%d)\n",sd->account_id);
//	printf("offset -> %d...\n",offset);

    set_char_online(-1, 99,sd->account_id);

	//search char.
	sprintf(tmp_sql, "SELECT `char_id` FROM `%s` WHERE `account_id` = '%d'",char_db, sd->account_id);
	if (mysql_query(&mysql_handle, tmp_sql)) {
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}
	sql_res = mysql_store_result(&mysql_handle);
	if (sql_res) {
		found_num = (int)mysql_num_rows(sql_res);
//		ShowInfo("number of chars: %d\n", found_num);
		i = 0;
		while((sql_row = mysql_fetch_row(sql_res))) {
			sd->found_char[i] = atoi(sql_row[0]);
			i++;
		}
		mysql_free_result(sql_res);
	}

//	printf("char fetching end (total: %d)....\n", found_num);

	for(i = found_num; i < 9; i++)
		sd->found_char[i] = -1;

	memset(WFIFOP(fd, 0), 0, offset + found_num * 106);
	WFIFOW(fd, 0) = 0x6b;
	WFIFOW(fd, 2) = offset + found_num * 106;

	if (save_log)
		ShowInfo("Request Char Data (\033[1;13m%d\033[0m):\n",sd->account_id);

	for(i = 0; i < found_num; i++) {
		mmo_char_fromsql_short(sd->found_char[i], char_dat);

		p = &char_dat[0];

		j = offset + (i * 106); // increase speed of code

		WFIFOL(fd,j) = p->char_id;
		WFIFOL(fd,j+4) = p->base_exp;
		WFIFOL(fd,j+8) = p->zeny;
		WFIFOL(fd,j+12) = p->job_exp;
		WFIFOL(fd,j+16) = p->job_level;

		WFIFOL(fd,j+20) = 0;
		WFIFOL(fd,j+24) = 0;
		WFIFOL(fd,j+28) = p->option;

		WFIFOL(fd,j+32) = p->karma;
		WFIFOL(fd,j+36) = p->manner;

		WFIFOW(fd,j+40) = p->status_point;
		WFIFOW(fd,j+42) = (p->hp > 0x7fff) ? 0x7fff : p->hp;
		WFIFOW(fd,j+44) = (p->max_hp > 0x7fff) ? 0x7fff : p->max_hp;
		WFIFOW(fd,j+46) = (p->sp > 0x7fff) ? 0x7fff : p->sp;
		WFIFOW(fd,j+48) = (p->max_sp > 0x7fff) ? 0x7fff : p->max_sp;
		WFIFOW(fd,j+50) = DEFAULT_WALK_SPEED; // p->speed;
		WFIFOW(fd,j+52) = p->class_;
		WFIFOW(fd,j+54) = p->hair;

		// pecopeco knights/crusaders crash fix
		if (p->class_ == 13 || p->class_ == 21 ||
			p->class_ == 4014 || p->class_ == 4022 ||
				p->class_ == 4036 || p->class_ == 4044)
			WFIFOW(fd,j+56) = 0;
		else WFIFOW(fd,j+56) = p->weapon;

		WFIFOW(fd,j+58) = p->base_level;
		WFIFOW(fd,j+60) = p->skill_point;
		WFIFOW(fd,j+62) = p->head_bottom;
		WFIFOW(fd,j+64) = p->shield;
		WFIFOW(fd,j+66) = p->head_top;
		WFIFOW(fd,j+68) = p->head_mid;
		WFIFOW(fd,j+70) = p->hair_color;
		WFIFOW(fd,j+72) = p->clothes_color;

		memcpy(WFIFOP(fd,j+74), p->name, NAME_LENGTH);

		WFIFOB(fd,j+98) = (p->str > 255) ? 255 : p->str;
		WFIFOB(fd,j+99) = (p->agi > 255) ? 255 : p->agi;
		WFIFOB(fd,j+100) = (p->vit > 255) ? 255 : p->vit;
		WFIFOB(fd,j+101) = (p->int_ > 255) ? 255 : p->int_;
		WFIFOB(fd,j+102) = (p->dex > 255) ? 255 : p->dex;
		WFIFOB(fd,j+103) = (p->luk > 255) ? 255 : p->luk;
		WFIFOB(fd,j+104) = p->char_num;
	}

	WFIFOSET(fd,WFIFOW(fd,2));
//	printf("mmo_char_send006b end..\n");
	return 0;
}

int parse_tologin(int fd) {
	int i;
	struct char_session_data *sd;

	// only login-server can have an access to here.
	// so, if it isn't the login-server, we disconnect the session.
	//session eof check!
	if(fd != login_fd)
		session[fd]->eof = 1;
	if(session[fd]->eof) {
		if (fd == login_fd) {
			ShowWarning("Connection to login-server lost (connection #%d).\n", fd);
			login_fd = -1;
		}
		do_close(fd);
		return 0;
	}

	sd = (struct char_session_data*)session[fd]->session_data;

	// hehe. no need to set user limit on SQL version. :P
	// but char limitation is good way to maintain server. :D
	while(RFIFOREST(fd) >= 2 && !session[fd]->eof) {
//		printf("parse_tologin : %d %d %x\n", fd, RFIFOREST(fd), RFIFOW(fd, 0));

		switch(RFIFOW(fd, 0)){
		case 0x2711:
			if (RFIFOREST(fd) < 3)
				return 0;
			if (RFIFOB(fd, 2)) {
				//printf("connect login server error : %d\n", RFIFOB(fd, 2));
				ShowError("Can not connect to login-server.\n");
				ShowError("The server communication passwords (default s1/p1) are probably invalid.\n");
				ShowError("Also, please make sure your login db has the correct coounication username/passwords and the gender of the account is S.\n");
				ShowError("The communication passwords are set in map_athena.conf and char_athena.conf\n");
				return 0;
				//exit(1); //fixed for server shutdown.
			}else {
				ShowStatus("Connected to login-server (connection #%d).\n", fd);
				if (kick_on_disconnect)
					set_all_offline();
				// if no map-server already connected, display a message...
				for(i = 0; i < MAX_MAP_SERVERS; i++)
					if (server_fd[i] > 0 && server[i].map[0][0]) // if map-server online and at least 1 map
						break;
				if (i == MAX_MAP_SERVERS)
					ShowStatus("Awaiting maps from map-server.\n");
			}
			RFIFOSKIP(fd, 3);
			break;

		case 0x2713:
			if(RFIFOREST(fd)<51)
				return 0;
			for(i = 0; i < fd_max; i++) {
				if (session[i] && (sd = (struct char_session_data*)session[i]->session_data) && sd->account_id == RFIFOL(fd,2)) {
					if (RFIFOB(fd,6) != 0) {
						WFIFOW(i,0) = 0x6c;
						WFIFOB(i,2) = 0x42;
						WFIFOSET(i,3);
					} else if (max_connect_user == 0 || count_users() < max_connect_user) {
//						if (max_connect_user == 0)
//							printf("max_connect_user (unlimited) -> accepted.\n");
//						else
//							printf("count_users(): %d < max_connect_user (%d) -> accepted.\n", count_users(), max_connect_user);
						sd->connect_until_time = (time_t)RFIFOL(fd,47);
						memcpy(sd->email, RFIFOP(fd, 7), 40);
						// send characters to player
						mmo_char_send006b(i, sd);
					} else if(isGM(sd->account_id) >= gm_allow_level) {
						sd->connect_until_time = (time_t)RFIFOL(fd,47);
						memcpy(sd->email, RFIFOP(fd, 7), 40);
						// send characters to player
						mmo_char_send006b(i, sd);
					} else {
						// refuse connection: too much online players
//						printf("count_users(): %d < max_connect_use (%d) -> fail...\n", count_users(), max_connect_user);
						WFIFOW(i,0) = 0x6c;
						WFIFOW(i,2) = 0;
						WFIFOSET(i,3);
					}
				}
			}
			RFIFOSKIP(fd,51);
			break;

		case 0x2717:
			if (RFIFOREST(fd) < 50)
				return 0;
			for(i = 0; i < fd_max; i++) {
				if (session[i] && (sd = (struct char_session_data*)session[i]->session_data)) {
					if (sd->account_id == RFIFOL(fd,2)) {
					sd->connect_until_time = (time_t)RFIFOL(fd,46);
					break;
					}
				}
			}
			RFIFOSKIP(fd,50);
			break;

		// login-server alive packet
		case 0x2718:
			if (RFIFOREST(fd) < 2)
				return 0;
			RFIFOSKIP(fd,2);
			break;

		// Receiving authentification from Freya-type login server (to avoid char->login->char)
		case 0x2719:
			if (RFIFOREST(fd) < 18)
				return 0;
			// to conserv a maximum of authentification, search if account is already authentified and replace it
			// that will reduce multiple connection too
			for(i = 0; i < AUTH_FIFO_SIZE; i++)
				if (auth_fifo[i].account_id == RFIFOL(fd,2))
					break;
			// if not found, use next value
			if (i == AUTH_FIFO_SIZE) {
				if (auth_fifo_pos >= AUTH_FIFO_SIZE)
					auth_fifo_pos = 0;
				i = auth_fifo_pos;
				auth_fifo_pos++;
			}
			//printf("auth_fifo set (auth #%d) - account: %d, secure: %08x-%08x\n", i, RFIFOL(fd,2), RFIFOL(fd,6), RFIFOL(fd,10));
			auth_fifo[i].account_id = RFIFOL(fd,2);
			auth_fifo[i].char_id = 0;
			auth_fifo[i].login_id1 = RFIFOL(fd,6);
			auth_fifo[i].login_id2 = RFIFOL(fd,10);
			auth_fifo[i].delflag = 2; // 0: auth_fifo canceled/void, 2: auth_fifo received from login/map server in memory, 1: connection authentified
			auth_fifo[i].char_pos = 0;
			auth_fifo[i].connect_until_time = 0; // unlimited/unknown time by default (not display in map-server)
			auth_fifo[i].ip = RFIFOL(fd,14);
			//auth_fifo[i].map_auth = 0;
			RFIFOSKIP(fd,18);
			break;

/*		case 0x2721:	// gm reply. I don't want to support this function.
			printf("0x2721:GM reply\n");
		  {
			int oldacc, newacc;
			unsigned char buf[64];
			if (RFIFOREST(fd) < 10)
				return 0;
			oldacc = RFIFOL(fd, 2);
			newacc = RFIFOL(fd, 6);
			RFIFOSKIP(fd, 10);
			if (newacc > 0) {
				for(i=0;i<char_num;i++){
					if(char_dat[i].account_id==oldacc)
						char_dat[i].account_id=newacc;
				}
			}
			WBUFW(buf,0)=0x2b0b;
			WBUFL(buf,2)=oldacc;
			WBUFL(buf,6)=newacc;
			mapif_sendall(buf,10);
//			printf("char -> map\n");
		  }
			break;
*/
		case 0x2723:	// changesex reply (modified by [Yor])
			if (RFIFOREST(fd) < 7)
				return 0;
		  {
			int acc, sex;
			unsigned char buf[16];

			acc = RFIFOL(fd,2);
			sex = RFIFOB(fd,6);
			RFIFOSKIP(fd, 7);
			if (acc > 0) {
				sprintf(tmp_sql, "SELECT `char_id`,`class`,`skill_point`,`guild_id` FROM `%s` WHERE `account_id` = '%d'",char_db, acc);
				if (mysql_query(&mysql_handle, tmp_sql)) {
					ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
					ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
				}
				sql_res = mysql_store_result(&mysql_handle);

				while(sql_res && (sql_row = mysql_fetch_row(sql_res))) {
						int char_id, guild_id, jobclass, skill_point, class_;
						char_id = atoi(sql_row[0]);
						jobclass = atoi(sql_row[1]);
						skill_point = atoi(sql_row[2]);
						guild_id = atoi(sql_row[3]);
						class_ = jobclass;
						if (jobclass == 19 || jobclass == 20 ||
						    jobclass == 4020 || jobclass == 4021 ||
						    jobclass == 4042 || jobclass == 4043) {
							// job modification
							if (jobclass == 19 || jobclass == 20) {
								class_ = (sex) ? 19 : 20;
							} else if (jobclass == 4020 || jobclass == 4021) {
								class_ = (sex) ? 4020 : 4021;
							} else if (jobclass == 4042 || jobclass == 4043) {
								class_ = (sex) ? 4042 : 4043;
							}
							// remove specifical skills of classes 19,20 4020,4021 and 4042,4043
							sprintf(tmp_sql, "SELECT `lv` FROM `%s` WHERE `char_id` = '%d' AND `id` >= '315' AND `id` <= '330'",skill_db, char_id);
							if (mysql_query(&mysql_handle, tmp_sql)) {
								ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
								ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
							}
							sql_res = mysql_store_result(&mysql_handle);
							if (sql_res) {
								while(( sql_row = mysql_fetch_row(sql_res))) {
									skill_point += atoi(sql_row[0]);
								}
							}
							sprintf(tmp_sql, "DELETE FROM `%s` WHERE `char_id` = '%d' AND `id` >= '315' AND `id` <= '330'",skill_db, char_id);
							if (mysql_query(&mysql_handle, tmp_sql)) {
								ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
								ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
							}
						}
						// to avoid any problem with equipment and invalid sex, equipment is unequiped.
						sprintf(tmp_sql, "UPDATE `%s` SET `equip` = '0' WHERE `char_id` = '%d'",inventory_db, char_id);
						if (mysql_query(&mysql_handle, tmp_sql)) {
							ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
							ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
						}
						sprintf(tmp_sql, "UPDATE `%s` SET `class`='%d' , `skill_point`='%d' , `weapon`='0' , `shield='0' , `head_top`='0' , `head_mid`='0' , `head_bottom`='0' WHERE `char_id` = '%d'",char_db, class_, skill_point, char_id);
						if (mysql_query(&mysql_handle, tmp_sql)) {
							ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
							ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
						}

						if (guild_id)	//If there is a guild, update the guild_member data [Skotlex]
							inter_guild_sex_changed(guild_id, acc, char_id, sex);
					}
				}
				// disconnect player if online on char-server
				for(i = 0; i < fd_max; i++) {
					if (session[i] && (sd = (struct char_session_data*)session[i]->session_data)) {
						if (sd->account_id == acc) {
							session[i]->eof = 1;
							break;
						}
					}
				}

			WBUFW(buf,0) = 0x2b0d;
			WBUFL(buf,2) = acc;
			WBUFB(buf,6) = sex;

			mapif_sendall(buf, 7);
		  }
			break;

		// account_reg2変更通知
		case 0x2729:
			if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd,2))
				return 0;
		  {
			struct global_reg reg[ACCOUNT_REG2_NUM];
			unsigned char buf[4096];
			int j, p, acc;
			acc = RFIFOL(fd,4);
			for(p = 8, j = 0; p < RFIFOW(fd,2) && j < ACCOUNT_REG2_NUM; p += 288, j++) {
				memcpy(reg[j].str, RFIFOP(fd,p), 32);
				memcpy(reg[j].value, RFIFOP(fd,p+32), 256);
			}
			// set_account_reg2(acc,j,reg);
			// 同垢ログインを禁止していれば送る必要は無い
			memcpy(buf,RFIFOP(fd,0), RFIFOW(fd,2));
			WBUFW(buf,0) = 0x2b11;
			mapif_sendall(buf, WBUFW(buf,2));
			RFIFOSKIP(fd, RFIFOW(fd,2));
//			printf("char: save_account_reg_reply\n");
		  }
			break;

		// State change of account/ban notification (from login-server) by [Yor]
		case 0x2731:
			if (RFIFOREST(fd) < 11)
				return 0;
			// send to all map-servers to disconnect the player
		  {
			unsigned char buf[16];
			WBUFW(buf,0) = 0x2b14;
			WBUFL(buf,2) = RFIFOL(fd,2);
			WBUFB(buf,6) = RFIFOB(fd,6); // 0: change of statut, 1: ban
			WBUFL(buf,7) = RFIFOL(fd,7); // status or final date of a banishment
			mapif_sendall(buf, 11);
		  }
			// disconnect player if online on char-server
			for(i = 0; i < fd_max; i++) {
				if (session[i] && (sd = (struct char_session_data*)session[i]->session_data)) {
					if (sd->account_id == RFIFOL(fd,2)) {
						session[i]->eof = 1;
						break;
					}
				}
			}
			RFIFOSKIP(fd,11);
			break;

		case 0x2732:
			if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd,2))
				return 0;
		  {
			unsigned char buf[32000];
			if (gm_account != NULL)
				aFree(gm_account);
			gm_account = (struct gm_account*)aCalloc(sizeof(struct gm_account) * ((RFIFOW(fd,2) - 4) / 5), 1);
			GM_num = 0;
			for (i = 4; i < RFIFOW(fd,2); i = i + 5) {
				gm_account[GM_num].account_id = RFIFOL(fd,i);
				gm_account[GM_num].level = (int)RFIFOB(fd,i+4);
				//printf("GM account: %d -> level %d\n", gm_account[GM_num].account_id, gm_account[GM_num].level);
				GM_num++;
			}
			ShowStatus("From login-server: receiving information of %d GM accounts.\n", GM_num);
			// send new gm acccounts level to map-servers
			memcpy(buf, RFIFOP(fd,0), RFIFOW(fd,2));
			WBUFW(buf,0) = 0x2b15;
			mapif_sendall(buf, RFIFOW(fd,2));
		  }
			RFIFOSKIP(fd,RFIFOW(fd,2));
			break;

		// Receive GM accounts [Freya login server packet by Yor]
		case 0x2733:
		// add test here to remember that the login-server is Freya-type
		// sprintf (login_server_type, "Freya");
			if (RFIFOREST(fd) < 7)
				return 0;
			{
				int new_level = 0;
				for(i = 0; i < GM_num; i++)
					if (gm_account[i].account_id == RFIFOL(fd,2)) {
						if (gm_account[i].level != (int)RFIFOB(fd,6)) {
							gm_account[i].level = (int)RFIFOB(fd,6);
							new_level = 1;
						}
						break;
					}
				// if not found, add it
				if (i == GM_num) {
					// limited to 4000, because we send information to char-servers (more than 4000 GM accounts???)
					// int (id) + int (level) = 8 bytes * 4000 = 32k (limit of packets in windows)
					if (((int)RFIFOB(fd,6)) > 0 && GM_num < 4000) {
						if (GM_num == 0) {
							gm_account = (struct gm_account*)aMalloc(sizeof(struct gm_account));
						} else {
							gm_account = (struct gm_account*)aRealloc(gm_account, sizeof(struct gm_account) * (GM_num + 1));
						}
						gm_account[GM_num].account_id = RFIFOL(fd,2);
						gm_account[GM_num].level = (int)RFIFOB(fd,6);
						new_level = 1;
						GM_num++;
						if (GM_num >= 4000)
							ShowWarning("4000 GM accounts found. Next GM accounts are not readed.\n");
					}
				}
				if (new_level == 1) {
					ShowStatus("From login-server: receiving GM account information (%d: level %d).\n", RFIFOL(fd,2), (int)RFIFOB(fd,6));
					mapif_send_gmaccounts();

					//create_online_files(); // not change online file for only 1 player (in next timer, that will be done
					// send gm acccounts level to map-servers
				}
			}
			RFIFOSKIP(fd,7);
			break;

		//Login server request to kick a character out. [Skotlex]
		case 0x2734:
			if (RFIFOREST(fd) < 6)
				return 0;
			{
				struct online_char_data* character;
				int aid = RFIFOL(fd,2);
				if ((character = numdb_search(online_char_db, aid)) != NULL)
				{	//Kick out this player.
					if (character->server > -1)
					{	//Kick it from the map server it is on.
						mapif_disconnectplayer(server_fd[character->server], character->account_id, character->char_id, 2);
						add_timer(gettick()+15000, chardb_waiting_disconnect, character->account_id, 0);
						character->waiting_disconnect = 1;
					} else { //Manual kick from char server.
						struct char_session_data *tsd;
						int i;
						for(i = 0; i < fd_max; i++) {
							if (session[i] && (tsd = (struct char_session_data*)session[i]->session_data) && tsd->account_id == aid)
							{
								WFIFOW(i,0) = 0x81;
								WFIFOB(i,2) = 2;
								WFIFOSET(i,3);
								break;
							}
						}
						if (i == fd_max) //Shouldn't happen, but just in case.
							set_char_offline(99, aid);
					}
				}
				RFIFOSKIP(fd,6);
			}
			break;

		default:
			ShowError("Unknown packet 0x%04x from login server, disconnecting.\n", RFIFOW(fd, 0));
			session[fd]->eof = 1;
			return 0;
		}
	}

	RFIFOFLUSH(fd);

	return 0;
}

int search_mapserver(char *map, long ip, short port);

int parse_frommap(int fd) {
	int i = 0, j = 0;
	int id;
//	int auth_fifo_flag=0;

	// Sometimes fd=0, and it will cause server crash. Don't know why. :(
	if (fd <= 0) {
		ShowError("parse_frommap error fd=%d\n", fd);
		return 0;
	}

	for(id = 0; id < MAX_MAP_SERVERS; id++)
		if (server_fd[id] == fd)
			break;
	if(id == MAX_MAP_SERVERS)
		session[fd]->eof = 1;
	if(session[fd]->eof) {
		if (id < MAX_MAP_SERVERS) {
			memset(&server[id], 0, sizeof(struct mmo_map_server));
			ShowStatus("Map-server %d (session #%d) has disconnected.\n", id, fd);
			sprintf(tmp_sql, "DELETE FROM `ragsrvinfo` WHERE `index`='%d'", server_fd[id]);
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			server_fd[id] = -1;
			numdb_foreach(online_char_db,char_db_setoffline,id); //Tag relevant chars as 'in disconnected' server.
		}
		do_close(fd);
		return 0;
	}

	while(RFIFOREST(fd) >= 2 && !session[fd]->eof) {
//		printf("parse_frommap : %d %d %x\n", fd, RFIFOREST(fd), RFIFOW(fd,0));

		switch(RFIFOW(fd, 0)) {

		// map-server alive packet
		case 0x2718:
			if (RFIFOREST(fd) < 2)
				return 0;
			RFIFOSKIP(fd,2);
			break;

		case 0x2af7:
			RFIFOSKIP(fd,2);
			if (login_fd > 0) { // don't send request if no login-server
				WFIFOW(login_fd,0) = 0x2709;
				WFIFOSET(login_fd, 2);
			}
			break;

		// mapserver -> map names recv.
		case 0x2afa:
			if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd,2))
				return 0;
			memset(server[id].map, 0, sizeof(server[id].map));
			j = 0;
			for(i = 4; i < RFIFOW(fd,2); i += MAP_NAME_LENGTH) {
				memcpy(server[id].map[j], RFIFOP(fd,i), MAP_NAME_LENGTH);
//				printf("set map %d.%d : %s\n", id, j, server[id].map[j]);
				j++;
			}
			i = server[id].ip;
			{
				unsigned char *p = (unsigned char *)&server[id].ip;
				ShowStatus("Map-Server %d connected: %d maps, from IP %d.%d.%d.%d port %d.\n",
				       id, j, p[0], p[1], p[2], p[3], server[id].port);
				ShowStatus("Map-server %d loading complete.\n", id);
				if (kick_on_disconnect)
					set_all_offline();
				if (max_account_id != DEFAULT_MAX_ACCOUNT_ID || max_char_id != DEFAULT_MAX_CHAR_ID)
					mapif_send_maxid(max_account_id, max_char_id); //Send the current max ids to the server to keep in sync [Skotlex]
			}
			WFIFOW(fd,0) = 0x2afb;
			WFIFOB(fd,2) = 0;
			memcpy(WFIFOP(fd,3), wisp_server_name, NAME_LENGTH); // name for wisp to player
			WFIFOSET(fd,3+NAME_LENGTH);
			//WFIFOSET(fd,27);
			{
				unsigned char buf[16384];
				int x;
				if (j == 0) {
					ShowWarning("Map-Server %d have NO maps.\n", id);
				// Transmitting maps information to the other map-servers
				} else {
					WBUFW(buf,0) = 0x2b04;
					WBUFW(buf,2) = j * 16 + 10;
					WBUFL(buf,4) = server[id].ip;
					WBUFW(buf,8) = server[id].port;
					memcpy(WBUFP(buf,10), RFIFOP(fd,4), j * 16);
					mapif_sendallwos(fd, buf, WBUFW(buf,2));
				}
				// Transmitting the maps of the other map-servers to the new map-server
				for(x = 0; x < MAX_MAP_SERVERS; x++) {
					if (server_fd[x] > 0 && x != id) {
						WFIFOW(fd,0) = 0x2b04;
						WFIFOL(fd,4) = server[x].ip;
						WFIFOW(fd,8) = server[x].port;
						j = 0;
						for(i = 0; i < MAX_MAP_PER_SERVER; i++)
							if (server[x].map[i][0])
								memcpy(WFIFOP(fd,10+(j++)*16), server[x].map[i], 16);
						if (j > 0) {
							WFIFOW(fd,2) = j * 16 + 10;
							WFIFOSET(fd,WFIFOW(fd,2));
						}
					}
				}
			}
			RFIFOSKIP(fd,RFIFOW(fd,2));
			break;
		//Packet command is now used for sc_data request. [Skotlex]
		case 0x2afc:
			if (RFIFOREST(fd) < 10)
				return 0;
		{
			int aid, cid;
			aid = RFIFOL(fd,2);
			cid = RFIFOL(fd,6);
			RFIFOSKIP(fd, 10);
#ifdef ENABLE_SC_SAVING
			sprintf(tmp_sql, "SELECT type, tick, val1, val2, val3, val4 from `%s` WHERE `account_id` = '%d' AND `char_id`='%d'",
				scdata_db, aid, cid);
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
				break;
			}
			sql_res = mysql_store_result(&mysql_handle);
			if (sql_res) {
				struct status_change_data data;
				int count = 0;
				WFIFOW(fd, 0) = 0x2b1d;
				WFIFOL(fd, 4) = aid;
				WFIFOL(fd, 8) = cid;
				while((sql_row = mysql_fetch_row(sql_res)))
				{
					data.type = atoi(sql_row[0]);
					data.tick = atoi(sql_row[1]);
					data.val1 = atoi(sql_row[2]);
					data.val2 = atoi(sql_row[3]);
					data.val3 = atoi(sql_row[4]);
					data.val4 = atoi(sql_row[5]);
					memcpy(WFIFOP(fd, 14+count*sizeof(struct status_change_data)), &data, sizeof(struct status_change_data));
					count++;
				}
				if (count > 0)
				{
					WFIFOW(fd, 2) = 14 + count*sizeof(struct status_change_data);
					WFIFOW(fd, 12) = count;
					WFIFOSET(fd, WFIFOW(fd,2));

					//Clear the data once loaded.
					sprintf(tmp_sql, "DELETE FROM `%s` WHERE `account_id` = '%d' AND `char_id`='%d'", scdata_db, aid, cid);
					if (mysql_query(&mysql_handle, tmp_sql)) {
						ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
						ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
					}
				}
			}
#endif
			break;
		}
		//set MAP user count
		case 0x2afe:
			if (RFIFOREST(fd) < 4)
				return 0;
			if (RFIFOW(fd,2) != server[id].users) {
				server[id].users = RFIFOW(fd,2);
				ShowInfo("User Count: %d (Server: %d)\n", server[id].users, id);
			}
			RFIFOSKIP(fd, 4);
			break;
		// set MAP user
		case 0x2aff:
			if (RFIFOREST(fd) < 6 || RFIFOREST(fd) < RFIFOW(fd,2))
				return 0;
		{
			int i, aid, cid;
			struct online_char_data* character;

			numdb_foreach(online_char_db,char_db_setoffline,id); //Set all chars from this server as 'unknown'
			server[id].users = RFIFOW(fd,4);
			for(i = 0; i < server[id].users; i++) {
				aid = RFIFOL(fd,6+i*8);
				cid = RFIFOL(fd,6+i*8+4);
				character = numdb_search(online_char_db, aid);
				if (character == NULL)
				{
					character = aCalloc(1, sizeof(struct online_char_data));
					character->account_id = aid;
					character->char_id = cid;
					character->server = id;
					numdb_insert(online_char_db, aid, character);
				} else {
					if (character->server > -1 && character->server != id)
					{
						ShowNotice("Set map user: Character (%d:%d) marked on map server %d, but map server %d claims to have (%d:%d) online!\n",
							character->account_id, character->char_id, character->server, id, aid, cid);
						mapif_disconnectplayer(server_fd[character->server], character->account_id, character->char_id, 2);
					}
					character->server = id;
					character->char_id = cid;
				}

			}
			//If any chars remain in -2, they will be cleaned in the cleanup timer.
			RFIFOSKIP(fd,RFIFOW(fd,2));
			break;
		}
		// char saving
		case 0x2b01:
			i = 0;
			if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd,2))
				return 0;
			//check account
			sprintf(tmp_sql, "SELECT count(*) FROM `%s` WHERE `account_id` = '%d' AND `char_id`='%d'",char_db, RFIFOL(fd,4),RFIFOL(fd,8)); // TBR
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			sql_res = mysql_store_result(&mysql_handle);
			sql_row = sql_res?mysql_fetch_row(sql_res):NULL;
			if (sql_row) i = atoi(sql_row[0]);
			if (sql_res) mysql_free_result(sql_res);

			if (i == 1) {
				memcpy(&char_dat[0], RFIFOP(fd,12), sizeof(struct mmo_charstatus));
				mmo_char_tosql(RFIFOL(fd,8), char_dat);
				//save to DB
			}
			RFIFOSKIP(fd,RFIFOW(fd,2));
			break;

		// req char selection
		case 0x2b02:
			if (RFIFOREST(fd) < 18)
				return 0;

			if (auth_fifo_pos >= AUTH_FIFO_SIZE)
				auth_fifo_pos = 0;

			auth_fifo[auth_fifo_pos].account_id = RFIFOL(fd, 2);
			auth_fifo[auth_fifo_pos].char_id = 0;
			auth_fifo[auth_fifo_pos].login_id1 = RFIFOL(fd, 6);
			auth_fifo[auth_fifo_pos].login_id2 = RFIFOL(fd,10);
			auth_fifo[auth_fifo_pos].delflag = 2;
			auth_fifo[auth_fifo_pos].char_pos = 0;
			auth_fifo[auth_fifo_pos].connect_until_time = 0; // unlimited/unknown time by default (not display in map-server)
			auth_fifo[auth_fifo_pos].ip = RFIFOL(fd,14);
			auth_fifo_pos++;

			WFIFOW(fd, 0) = 0x2b03;
			WFIFOL(fd, 2) = RFIFOL(fd, 2);
			WFIFOB(fd, 6) = 0;
			WFIFOSET(fd, 7);

			RFIFOSKIP(fd, 18);
			break;

		// request "change map server"
		case 0x2b05:
			if (RFIFOREST(fd) < 49)
				return 0;
			{
				char name[MAP_NAME_LENGTH];
				int map_id, map_fd = -1;
				struct online_char_data* data;
				struct mmo_charstatus* char_data;

				WFIFOW(fd, 0) = 0x2b06;
				memcpy(WFIFOP(fd,2), RFIFOP(fd,2), 42);
				WFIFOSET(fd, 44);

				memcpy(name, RFIFOP(fd,18), MAP_NAME_LENGTH);
				name[MAP_NAME_LENGTH-1]= '\0';
				map_id = search_mapserver(name, RFIFOL(fd,38), RFIFOW(fd,42)); //Locate mapserver by ip and port.
				if (map_id >= 0)
					map_fd = server_fd[map_id];
				//Char should just had been saved before this packet, so this should be safe. [Skotlex]
				char_data = (struct mmo_charstatus*)numdb_search(char_db_,RFIFOL(fd,14));
				if (char_data == NULL)
				{	//Really shouldn't happen.
					mmo_char_fromsql(RFIFOL(fd,14), char_dat);
					char_data = char_dat;
				}
				//Tell the new map server about this player using Kevin's new auth packet. [Skotlex]
				if (map_fd>=0 && session[map_fd] && char_data)
				{	//Send the map server the auth of this player.
					char_data->sex = RFIFOB(fd,44);
					//Update the "last map" as this is where the player must be spawned on the new map server.
					memcpy(char_data->last_point.map, RFIFOP(fd,18), MAP_NAME_LENGTH);
					char_data->last_point.x = RFIFOW(fd,34);
					char_data->last_point.y = RFIFOW(fd,36);

					WFIFOW(map_fd,0) = 0x2afd;
					WFIFOW(map_fd,2) = 20 + sizeof(struct mmo_charstatus);
					WFIFOL(map_fd,4) = RFIFOL(fd, 2); //Account ID
					WFIFOL(map_fd,8) = RFIFOL(fd, 6); //Login1
					WFIFOL(map_fd,16) = RFIFOL(fd,10); //Login2
					WFIFOL(map_fd,12) = (unsigned long)0; //TODO: connect_until_time, how do I figure it out right now?
					memcpy(WFIFOP(map_fd,20), char_data, sizeof(struct mmo_charstatus));
					WFIFOSET(map_fd, WFIFOW(map_fd,2));
					data = numdb_search(online_char_db, RFIFOL(fd, 2));
					if (data) //This check should really never fail...
						data->server = map_id; //Update server where char is.
				}
				RFIFOSKIP(fd, 49);
			}
			break;

		// char name check
		case 0x2b08:
			if (RFIFOREST(fd) < 6)
				return 0;

			sprintf(tmp_sql, "SELECT `name` FROM `%s` WHERE `char_id`='%d'", char_db, RFIFOL(fd,2));
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}

			sql_res = mysql_store_result(&mysql_handle);
			sql_row = sql_res?mysql_fetch_row(sql_res):NULL;

			WFIFOW(fd,0) = 0x2b09;
			WFIFOL(fd,2) = RFIFOL(fd,2);

			if (sql_row)
				memcpy(WFIFOP(fd,6), sql_row[0], NAME_LENGTH);
			else
				memcpy(WFIFOP(fd,6), unknown_char_name, NAME_LENGTH);
			mysql_free_result(sql_res);

			WFIFOSET(fd,30);

			RFIFOSKIP(fd,6);
			break;

/*		// I want become GM - fuck!
		case 0x2b0a:
			if(RFIFOREST(fd)<4)
				return 0;
			if(RFIFOREST(fd)<RFIFOW(fd,2))
				return 0;
			memcpy(WFIFOP(login_fd,2),RFIFOP(fd,2),RFIFOW(fd,2)-2);
			WFIFOW(login_fd,0)=0x2720;
			WFIFOSET(login_fd,RFIFOW(fd,2));
//			printf("char : change gm -> login %d %s %d\n", RFIFOL(fd, 4), RFIFOP(fd, 8), RFIFOW(fd, 2));
			RFIFOSKIP(fd, RFIFOW(fd, 2));
			break;
		*/

		// account_reg2 saving, forward to login server
		case 0x2b10:
			if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd,2))
				return 0;
		  {
			if (login_fd > 0) { // don't send request if no login-server
				memcpy(WFIFOP(login_fd,0), RFIFOP(fd,0), RFIFOW(fd,2));
				WFIFOW(login_fd, 0) = 0x2728;
				WFIFOSET(login_fd, WFIFOW(login_fd,2));
			}
			// ワールドへの同垢ログインがなければmapサーバーに送る必要はない
			//memcpy(buf,RFIFOP(fd,0),RFIFOW(fd,2));
			//WBUFW(buf,0)=0x2b11;
			//mapif_sendall(buf,WBUFW(buf,2));
			RFIFOSKIP(fd,RFIFOW(fd,2));
		  }
			break;

		// Map server send information to change an email of an account -> login-server
		case 0x2b0c:
			if (RFIFOREST(fd) < 86)
				return 0;
			if (login_fd > 0) { // don't send request if no login-server
				memcpy(WFIFOP(login_fd,0), RFIFOP(fd,0), 86); // 0x2722 <account_id>.L <actual_e-mail>.40B <new_e-mail>.40B
				WFIFOW(login_fd,0) = 0x2722;
				WFIFOSET(login_fd, 86);
			}
			RFIFOSKIP(fd, 86);
			break;

		// Receiving from map-server a status change resquest. Transmission to login-server (by Yor)
		case 0x2b0e:
			if (RFIFOREST(fd) < 44)
				return 0;
		  {
			char character_name[NAME_LENGTH], t_name[NAME_LENGTH*2];
			int acc = RFIFOL(fd,2); // account_id of who ask (-1 if nobody)
			memcpy(character_name, RFIFOP(fd,6), NAME_LENGTH);
			character_name[NAME_LENGTH-1] = '\0';
			jstrescapecpy(t_name, character_name); //Escape string for sql use... [Skotlex]
			// prepare answer
			WFIFOW(fd,0) = 0x2b0f; // answer
			WFIFOL(fd,2) = acc; // who want do operation
			WFIFOW(fd,30) = RFIFOW(fd, 30); // type of operation: 1-block, 2-ban, 3-unblock, 4-unban
			sprintf(tmp_sql, "SELECT `account_id`,`name` FROM `%s` WHERE `name` = '%s'",char_db, t_name);

			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}

			sql_res = mysql_store_result(&mysql_handle);

			if (sql_res) {
				if (mysql_num_rows(sql_res)) {
					sql_row = mysql_fetch_row(sql_res);
					memcpy(WFIFOP(fd,6), sql_row[1], NAME_LENGTH); // put correct name if found
					WFIFOW(fd,32) = 0; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
					switch(RFIFOW(fd, 30)) {
					case 1: // block
						if (acc == -1 || isGM(acc) >= isGM(atoi(sql_row[0]))) {
							if (login_fd > 0) { // don't send request if no login-server
								WFIFOW(login_fd,0) = 0x2724;
								WFIFOL(login_fd,2) = atoi(sql_row[0]); // account value
								WFIFOL(login_fd,6) = 5; // status of the account
								WFIFOSET(login_fd, 10);
//								printf("char : status -> login: account %d, status: %d \n", char_dat[i].account_id, 5);
							} else
								WFIFOW(fd,32) = 3; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						} else
							WFIFOW(fd,32) = 2; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						break;
					case 2: // ban
						if (acc == -1 || isGM(acc) >= isGM(atoi(sql_row[0]))) {
							if (login_fd > 0) { // don't send request if no login-server
								WFIFOW(login_fd, 0) = 0x2725;
								WFIFOL(login_fd, 2) = atoi(sql_row[0]); // account value
								WFIFOW(login_fd, 6) = RFIFOW(fd,32); // year
								WFIFOW(login_fd, 8) = RFIFOW(fd,34); // month
								WFIFOW(login_fd,10) = RFIFOW(fd,36); // day
								WFIFOW(login_fd,12) = RFIFOW(fd,38); // hour
								WFIFOW(login_fd,14) = RFIFOW(fd,40); // minute
								WFIFOW(login_fd,16) = RFIFOW(fd,42); // second
								WFIFOSET(login_fd,18);
//								printf("char : status -> login: account %d, ban: %dy %dm %dd %dh %dmn %ds\n",
//								       char_dat[i].account_id, (short)RFIFOW(fd,32), (short)RFIFOW(fd,34), (short)RFIFOW(fd,36), (short)RFIFOW(fd,38), (short)RFIFOW(fd,40), (short)RFIFOW(fd,42));
							} else
								WFIFOW(fd,32) = 3; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						} else
							WFIFOW(fd,32) = 2; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						break;
					case 3: // unblock
						if (acc == -1 || isGM(acc) >= isGM(atoi(sql_row[0]))) {
							if (login_fd > 0) { // don't send request if no login-server
								WFIFOW(login_fd,0) = 0x2724;
								WFIFOL(login_fd,2) = atoi(sql_row[0]); // account value
								WFIFOL(login_fd,6) = 0; // status of the account
								WFIFOSET(login_fd, 10);
//								printf("char : status -> login: account %d, status: %d \n", char_dat[i].account_id, 0);
							} else
								WFIFOW(fd,32) = 3; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						} else
							WFIFOW(fd,32) = 2; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						break;
					case 4: // unban
						if (acc == -1 || isGM(acc) >= isGM(atoi(sql_row[0]))) {
							if (login_fd > 0) { // don't send request if no login-server
								WFIFOW(login_fd, 0) = 0x272a;
								WFIFOL(login_fd, 2) = atoi(sql_row[0]); // account value
								WFIFOSET(login_fd, 6);
//								printf("char : status -> login: account %d, unban request\n", char_dat[i].account_id);
							} else
								WFIFOW(fd,32) = 3; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						} else
							WFIFOW(fd,32) = 2; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						break;
					case 5: // changesex
						if (acc == -1 || isGM(acc) >= isGM(atoi(sql_row[0]))) {
							if (login_fd > 0) { // don't send request if no login-server
								WFIFOW(login_fd, 0) = 0x2727;
								WFIFOL(login_fd, 2) = atoi(sql_row[0]); // account value
								WFIFOSET(login_fd, 6);
//								printf("char : status -> login: account %d, change sex request\n", char_dat[i].account_id);
							} else
								WFIFOW(fd,32) = 3; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						} else
							WFIFOW(fd,32) = 2; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
						break;
					}
				} else {
					// character name not found
					memcpy(WFIFOP(fd,6), character_name, NAME_LENGTH);
					WFIFOW(fd,32) = 1; // answer: 0-login-server resquest done, 1-player not found, 2-gm level too low, 3-login-server offline
				}
				// send answer if a player ask, not if the server ask
				if (acc != -1) {
					WFIFOSET(fd, 34);
				}
			}
		  }
			RFIFOSKIP(fd, 44);
			break;

		// Recieve rates [Wizputer]
		case 0x2b16:
			if (RFIFOREST(fd) < 6 || RFIFOREST(fd) < RFIFOW(fd,8))
				return 0;
			sprintf(tmp_sql, "INSERT INTO `ragsrvinfo` SET `index`='%d',`name`='%s',`exp`='%d',`jexp`='%d',`drop`='%d',`motd`='%s'",
			        fd, server_name, RFIFOW(fd,2), RFIFOW(fd,4), RFIFOW(fd,6), RFIFOP(fd,10));
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			RFIFOSKIP(fd,RFIFOW(fd,8));
			break;


		// Character disconnected set online 0 [Wizputer]
		case 0x2b17:
			if (RFIFOREST(fd) < 6 )
				return 0;
			//printf("Setting %d char offline\n",RFIFOL(fd,2));
			set_char_offline(RFIFOL(fd,2),RFIFOL(fd,6));
			RFIFOSKIP(fd,10);
			break;
		// Reset all chars to offline [Wizputer]
		case 0x2b18:
			ShowNotice("Map server [%d] requested to set all characters offline.\n", id);
			set_all_offline();
			RFIFOSKIP(fd,2);
			break;
		// Character set online [Wizputer]
		case 0x2b19:
			if (RFIFOREST(fd) < 6 )
				return 0;
			set_char_online(id, RFIFOL(fd,2),RFIFOL(fd,6));
			RFIFOSKIP(fd,10);
			break;

		// Request sending of fame list
		case 0x2b1a:
			if (RFIFOREST(fd) < 2)
				return 0;
		{
			int len = 8, num = 0;
			unsigned char buf[32000];
			struct fame_list fame_item;

			WBUFW(buf,0) = 0x2b1b;
			sprintf(tmp_sql, "SELECT `char_id`,`fame`, `name` FROM `%s` WHERE `fame`>0 AND (`class`='10' OR `class`='4011'OR `class`='4033') ORDER BY `fame` DESC LIMIT 0,10", char_db);
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			sql_res = mysql_store_result(&mysql_handle);
			if (sql_res) {
				while((sql_row = mysql_fetch_row(sql_res))) {
					fame_item.id = atoi(sql_row[0]);
					fame_item.fame = atoi(sql_row[1]);
					strncpy(fame_item.name, sql_row[2], NAME_LENGTH);

					memcpy(WBUFP(buf,len), &fame_item, sizeof(struct fame_list));
					len += sizeof(struct fame_list);
					if (++num == 10)
						break;
				}
			}
   		mysql_free_result(sql_res);
			WBUFW(buf, 6) = len; //Blacksmith block size

			num = 0;
			sprintf(tmp_sql, "SELECT `char_id`,`fame`,`name` FROM `%s` WHERE `fame`>0 AND (`class`='18' OR `class`='4019' OR `class`='4041') ORDER BY `fame` DESC LIMIT 0,10", char_db);
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			sql_res = mysql_store_result(&mysql_handle);
			if (sql_res) {
				while((sql_row = mysql_fetch_row(sql_res))) {
					fame_item.id = atoi(sql_row[0]);
					fame_item.fame = atoi(sql_row[1]);
					strncpy(fame_item.name, sql_row[2], NAME_LENGTH);
					memcpy(WBUFP(buf,len), &fame_item, sizeof(struct fame_list));
					len += sizeof(struct fame_list);
					if (++num == 10)
						break;
				}
			}
			mysql_free_result(sql_res);
			WBUFW(buf, 4) = len; //Alchemist block size

			num = 0;
			sprintf(tmp_sql, "SELECT `char_id`,`fame`,`name` FROM `%s` WHERE `fame`>0 AND `class`='4046' ORDER BY `fame` DESC LIMIT 0,10", char_db);
			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			sql_res = mysql_store_result(&mysql_handle);
			if (sql_res) {
				while((sql_row = mysql_fetch_row(sql_res))) {
					fame_item.id = atoi(sql_row[0]);
					fame_item.fame = atoi(sql_row[1]);
					strncpy(fame_item.name, sql_row[2], NAME_LENGTH);
					memcpy(WBUFP(buf,len), &fame_item, sizeof(struct fame_list));
					len += sizeof(struct fame_list);
					if (++num == 10)
						break;
				}
			}
			mysql_free_result(sql_res);
			WBUFW(buf, 2) = len; //Total packet length

			mapif_sendall(buf, len);
			RFIFOSKIP(fd,2);
			break;
		}
		//Request saving sc_data of a player. [Skotlex]
		case 0x2b1c:
			if (RFIFOREST(fd) < 4 || RFIFOREST(fd) < RFIFOW(fd,2))
				return 0;
		{
#ifdef ENABLE_SC_SAVING
			int count, aid, cid, i;
			struct status_change_data data;

			aid = RFIFOL(fd, 4);
			cid = RFIFOL(fd, 8);
			count = RFIFOW(fd, 12);

			sprintf(tmp_sql, "INSERT INTO `%s` (`account_id`, `char_id`, `type`, `tick`, `val1`, `val2`, `val3`, `val4`) VALUES ", scdata_db);

			for (i = 0; i < count; i++)
			{
				memcpy (&data, RFIFOP(fd, 14+i*sizeof(struct status_change_data)), sizeof(struct status_change_data));
				sprintf (tmp_sql, "%s ('%d','%d','%hu','%d','%d','%d','%d','%d'),", tmp_sql, aid, cid,
					data.type, data.tick, data.val1, data.val2, data.val3, data.val4);
			}
			if (count > 0)
			{
				tmp_sql[strlen(tmp_sql)-1] = '\0'; //Remove final comma.
				if (mysql_query(&mysql_handle, tmp_sql)) {
					ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
					ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
				}
			}
#endif
			RFIFOSKIP(fd, RFIFOW(fd, 2));
			break;
		}

		default:
			// inter server - packet
			{
				int r = inter_parse_frommap(fd);
				if (r == 1) break;		// processed
				if (r == 2) return 0;	// need more packet
			}

			// no inter server packet. no char server packet -> disconnect
			ShowError("Unknown packet 0x%04x from map server, disconnecting.\n", RFIFOW(fd,0));
			session[fd]->eof = 1;
			return 0;
		}
	}
	return 0;
}

int search_mapserver(char *map, long ip, short port) {
	int i, j;
	char temp_map[MAP_NAME_LENGTH];
	int temp_map_len;

//	printf("Searching the map-server for map '%s'... ", map);
	strncpy(temp_map, map, MAP_NAME_LENGTH-1);
	temp_map[MAP_NAME_LENGTH-1] = '\0';
	if (strchr(temp_map, '.') != NULL)
		temp_map[strchr(temp_map, '.') - temp_map + 1] = '\0'; // suppress the '.gat', but conserve the '.' to be sure of the name of the map

	temp_map_len = strlen(temp_map);
	for(i = 0; i < MAX_MAP_SERVERS; i++)
		if (server_fd[i] > 0)
			for (j = 0; server[i].map[j][0]; j++)
				//printf("%s : %s = %d\n", server[i].map[j], map, strncmp(server[i].map[j], temp_map, temp_map_len));
				if (strncmp(server[i].map[j], temp_map, temp_map_len) == 0) {
					if (ip > 0 && server[i].ip != ip)
						continue;
					if (port > 0 && server[i].port != port)
						continue;
					return i;
				}

//	printf("not found.\n");
	return -1;
}

int char_mapif_init(int fd) {
	return inter_mapif_init(fd);
}

//-----------------------------------------------------
// Test to know if an IP come from LAN or WAN. by [Yor]
//-----------------------------------------------------
int lan_ip_check(unsigned char *p){
	int i;
	int lancheck = 1;
	int subneti[4];
	unsigned int k0, k1, k2, k3;

	sscanf(lan_map_ip, "%d.%d.%d.%d", &k0, &k1, &k2, &k3);
	subneti[0] = k0; subneti[1] = k1; subneti[2] = k2; subneti[3] = k3;

//	printf("lan_ip_check: to compare: %d.%d.%d.%d, network: %d.%d.%d.%d/%d.%d.%d.%d\n",
//	       p[0], p[1], p[2], p[3],
//	       subneti[0], subneti[1], subneti[2], subneti[3],
//	       subnetmaski[0], subnetmaski[1], subnetmaski[2], subnetmaski[3]);
	for(i = 0; i < 4; i++) {
		if ((subneti[i] & subnetmaski[i]) != (p[i] & subnetmaski[i])) {
			lancheck = 0;
			break;
		}
	}
//	printf("LAN test (result): %s source\033[0m.\n", (lancheck) ? "\033[1;36mLAN" : "\033[1;32mWAN");
	return lancheck;
}

int parse_char(int fd) {
	int i, ch = 0;
	char email[40];
	unsigned char buf[64];
	unsigned short cmd;
	int map_fd;
	struct char_session_data *sd;
	unsigned char *p = (unsigned char *) &session[fd]->client_addr.sin_addr;

	sd = (struct char_session_data*)session[fd]->session_data;

	if(login_fd < 0)
		session[fd]->eof = 1;
	if(session[fd]->eof) {
		if (fd == login_fd)
			login_fd = -1;
		if (sd != NULL)
		{
			struct online_char_data* data = numdb_search(online_char_db, sd->account_id);
			if (!data || data->server== -1) //If it is not in any server, send it offline. [Skotlex]
				set_char_offline(99,sd->account_id);
		}
		do_close(fd);
		return 0;
	}

	while(RFIFOREST(fd) >= 2 && !session[fd]->eof) {
		cmd = RFIFOW(fd,0);
		// crc32のスキップ用
		if(	sd==NULL			&&	// 未ログインor管理パケット
			RFIFOREST(fd)>=4	&&	// 最低バイト数制限 ＆ 0x7530,0x7532管理パケ除去
			RFIFOREST(fd)<=21	&&	// 最大バイト数制限 ＆ サーバーログイン除去
			cmd!=0x20b	&&	// md5通知パケット除去
			(RFIFOREST(fd)<6 || RFIFOW(fd,4)==0x65)	){	// 次に何かパケットが来てるなら、接続でないとだめ
			RFIFOSKIP(fd,4);
			cmd = RFIFOW(fd,0);
			ShowDebug("parse_char : %d crc32 skipped\n",fd);
			if(RFIFOREST(fd)==0)
				return 0;
		}

//For use in packets that depend on an sd being present [Skotlex]
#define FIFOSD_CHECK(rest) { if(RFIFOREST(fd) < rest) return 0; if (sd==NULL) { RFIFOSKIP(fd,rest); return 0; } }

		switch(cmd){
		case 0x20b: //20040622 encryption ragexe correspondence
			if (RFIFOREST(fd) < 19)
				return 0;
			RFIFOSKIP(fd,19);
			break;

		case 0x65: // request to connect
			ShowInfo("request connect - account_id:%d/login_id1:%d/login_id2:%d\n", RFIFOL(fd, 2), RFIFOL(fd, 6), RFIFOL(fd, 10));
			if (RFIFOREST(fd) < 17)
				return 0;
		{
			if (sd == NULL) {
				CREATE(session[fd]->session_data, struct char_session_data, 1);
				sd = (struct char_session_data*)session[fd]->session_data;
				sd->connect_until_time = 0; // unknow or illimited (not displaying on map-server)
			}
			sd->account_id = RFIFOL(fd, 2);
			sd->login_id1 = RFIFOL(fd, 6);
			sd->login_id2 = RFIFOL(fd, 10);
			sd->sex = RFIFOB(fd, 16);

			WFIFOL(fd, 0) = RFIFOL(fd, 2);
			WFIFOSET(fd, 4);

			for(i = 0; i < AUTH_FIFO_SIZE; i++) {
				if (auth_fifo[i].account_id == sd->account_id &&
				    auth_fifo[i].login_id1 == sd->login_id1 &&
#if CMP_AUTHFIFO_LOGIN2 != 0
				    auth_fifo[i].login_id2 == sd->login_id2 && // relate to the versions higher than 18
#endif
				    (!check_ip_flag || auth_fifo[i].ip == session[fd]->client_addr.sin_addr.s_addr) &&
				    auth_fifo[i].delflag == 2) {
				auth_fifo[i].delflag = 1;

				if (online_check)
				{	// check if character is not online already. [Skotlex]
					struct online_char_data* character;
					character = numdb_search(online_char_db, sd->account_id);

					if (character)
					{
						if (character->server > -1)
						{	//Character already online. KICK KICK KICK
							mapif_disconnectplayer(server_fd[character->server], character->account_id, character->char_id, 2);
							add_timer(gettick()+15000, chardb_waiting_disconnect, character->account_id, 0);
							character->waiting_disconnect = 1;
						/* Not a good idea because this would trigger when you do a char-change from the map server! [Skotlex]
						} else { //Kick from char server.
							struct char_session_data *tsd;
							int i;
							for(i = 0; i < fd_max; i++) {
								if (session[i] && i != fd && (tsd = (struct char_session_data*)session[i]->session_data) && tsd->account_id == sd->account_id)
								{
									WFIFOW(i,0) = 0x81;
									WFIFOB(i,2) = 2;
									WFIFOSET(i,3);
									break;
								}
							if (i == fd_max) //Shouldn't happen, but just in case.
								set_char_offline(99, sd->account_id);
							}
						*/
							WFIFOW(fd,0) = 0x81;
							WFIFOB(fd,2) = 8;
							WFIFOSET(fd,3);
							break;
						}
					}
				}

				if (max_connect_user == 0 || count_users() < max_connect_user) {
					if (login_fd > 0) { // don't send request if no login-server
						// request to login-server to obtain e-mail/time limit
						WFIFOW(login_fd,0) = 0x2716;
						WFIFOL(login_fd,2) = sd->account_id;
						WFIFOSET(login_fd,6);
					}
					// send characters to player
					mmo_char_send006b(fd, sd);
				} else {
					// refuse connection (over populated)
					WFIFOW(fd,0) = 0x6c;
					WFIFOW(fd,2) = 0;
					WFIFOSET(fd,3);
				}
//				printf("connection request> set delflag 1(o:2)- account_id:%d/login_id1:%d(fifo_id:%d)\n", sd->account_id, sd->login_id1, i);
				break;
				}
			}
			if (i == AUTH_FIFO_SIZE) {
				if (login_fd > 0) { // don't send request if no login-server
					WFIFOW(login_fd,0) = 0x2712; // ask login-server to authentify an account
					WFIFOL(login_fd,2) = sd->account_id;
					WFIFOL(login_fd,6) = sd->login_id1;
					WFIFOL(login_fd,10) = sd->login_id2;
					WFIFOB(login_fd,14) = sd->sex;
					WFIFOL(login_fd,15) = session[fd]->client_addr.sin_addr.s_addr;
					WFIFOSET(login_fd,19);
				} else { // if no login-server, we must refuse connection
					WFIFOW(fd,0) = 0x6c;
					WFIFOW(fd,2) = 0;
					WFIFOSET(fd,3);
				}
			}
		}
			RFIFOSKIP(fd, 17);
			break;

		case 0x66: // char select
			FIFOSD_CHECK(3);

			sprintf(tmp_sql, "SELECT `char_id` FROM `%s` WHERE `account_id`='%d' AND `char_num`='%d'",char_db, sd->account_id, RFIFOB(fd, 2));
			RFIFOSKIP(fd, 3);

			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}
			sql_res = mysql_store_result(&mysql_handle);
			sql_row = sql_res?mysql_fetch_row(sql_res):NULL;

			if (sql_row)
			{
				mmo_char_fromsql(atoi(sql_row[0]), char_dat);
				char_dat[0].sex = sd->sex;
			} else {
				mysql_free_result(sql_res);
				break;
			}

			if (log_char) {
				char escaped_name[NAME_LENGTH*2];
				sprintf(tmp_sql,"INSERT INTO `%s`(`time`, `account_id`,`char_num`,`name`) VALUES (NOW(), '%d', '%d', '%s')",
					charlog_db, sd->account_id, RFIFOB(fd, 2), jstrescapecpy(escaped_name, char_dat[0].name));
				//query
				if(mysql_query(&mysql_handle, tmp_sql)) {
					ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
					ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
				}
			}
			ShowInfo("Selected char: (Account %d: %d - %s)" RETCODE, sd->account_id, RFIFOB(fd, 2), char_dat[0].name);

			i = search_mapserver(char_dat[0].last_point.map, -1, -1);

			// if map is not found, we check major cities
			if (i < 0) {
				ShowWarning("Unable to find map-server for %s, resorting to sending to a major city.\n", char_dat[0].last_point.map);
				if ((i = search_mapserver("prontera.gat", -1, -1)) >= 0) { // check is done without 'gat'.
					memcpy(char_dat[0].last_point.map, "prontera.gat", MAP_NAME_LENGTH);
					char_dat[0].last_point.x = 273; // savepoint coordonates
					char_dat[0].last_point.y = 354;
				} else if ((i = search_mapserver("geffen.gat", -1, -1)) >= 0) { // check is done without 'gat'.
					memcpy(char_dat[0].last_point.map, "geffen.gat", MAP_NAME_LENGTH);
					char_dat[0].last_point.x = 120; // savepoint coordonates
					char_dat[0].last_point.y = 100;
				} else if ((i = search_mapserver("morocc.gat", -1, -1)) >= 0) { // check is done without 'gat'.
					memcpy(char_dat[0].last_point.map, "morocc.gat", MAP_NAME_LENGTH);
					char_dat[0].last_point.x = 160; // savepoint coordonates
					char_dat[0].last_point.y = 94;
				} else if ((i = search_mapserver("alberta.gat", -1, -1)) >= 0) { // check is done without 'gat'.
					memcpy(char_dat[0].last_point.map, "alberta.gat", MAP_NAME_LENGTH);
					char_dat[0].last_point.x = 116; // savepoint coordonates
					char_dat[0].last_point.y = 57;
				} else if ((i = search_mapserver("payon.gat", -1, -1)) >= 0) { // check is done without 'gat'.
					memcpy(char_dat[0].last_point.map, "payon.gat", MAP_NAME_LENGTH);
					char_dat[0].last_point.x = 87; // savepoint coordonates
					char_dat[0].last_point.y = 117;
				} else if ((i = search_mapserver("izlude.gat", -1, -1)) >= 0) { // check is done without 'gat'.
					memcpy(char_dat[0].last_point.map, "izlude.gat", MAP_NAME_LENGTH);
					char_dat[0].last_point.x = 94; // savepoint coordonates
					char_dat[0].last_point.y = 103;
				} else {
					int j;
					// get first online server
					i = 0;
					for(j = 0; j < MAX_MAP_SERVERS; j++)
						if (server_fd[j] > 0 && server[j].map[0][0])  {
							i = j;
							ShowDebug("Map-server #%d found with a map: '%s'.\n", j, server[j].map[0]);
							break;
						}
					// if no map-servers are connected, we send: server closed
					if (j == MAX_MAP_SERVERS) {
						WFIFOW(fd,0) = 0x81;
						WFIFOB(fd,2) = 1; // 01 = Server closed
						WFIFOSET(fd,3);
						break;
					}
				}
			}
			WFIFOW(fd, 0) =0x71;
			WFIFOL(fd, 2) =char_dat[0].char_id;
			memcpy(WFIFOP(fd, 6), char_dat[0].last_point.map, MAP_NAME_LENGTH);
			//Lan check added by Kashy
			if (lan_ip_check(p))
				WFIFOL(fd, 22) = inet_addr(lan_map_ip);
			else
				WFIFOL(fd, 22) = server[i].ip;
			WFIFOW(fd, 26) = server[i].port;
			WFIFOSET(fd, 28);
			if (auth_fifo_pos >= AUTH_FIFO_SIZE) {
				auth_fifo_pos = 0;
			}
			auth_fifo[auth_fifo_pos].account_id = sd->account_id;
			auth_fifo[auth_fifo_pos].char_id = char_dat[0].char_id;
			auth_fifo[auth_fifo_pos].login_id1 = sd->login_id1;
			auth_fifo[auth_fifo_pos].login_id2 = sd->login_id2;
			auth_fifo[auth_fifo_pos].delflag = 0;
			auth_fifo[auth_fifo_pos].char_pos = 0;
			auth_fifo[auth_fifo_pos].sex = sd->sex;
			auth_fifo[auth_fifo_pos].connect_until_time = sd->connect_until_time;
			auth_fifo[auth_fifo_pos].ip = session[fd]->client_addr.sin_addr.s_addr;

			//Send NEW auth packet [Kevin]
			if ((map_fd = server_fd[i]) < 1 || session[map_fd] == NULL)
			{
				ShowError("parse_char: Attempting to write to invalid session %d! Map Server #%d disconnected.\n", map_fd, i);
				server_fd[i] = -1;
				memset(&server[i], 0, sizeof(struct mmo_map_server));
				break;
			}

			WFIFOW(map_fd,0) = 0x2afd;
			WFIFOW(map_fd,2) = 20 + sizeof(struct mmo_charstatus);
			WFIFOL(map_fd,4) = auth_fifo[auth_fifo_pos].account_id;
			WFIFOL(map_fd,8) = auth_fifo[auth_fifo_pos].login_id1;
			WFIFOL(map_fd,16) = auth_fifo[auth_fifo_pos].login_id2;
			WFIFOL(map_fd,12) = (unsigned long)auth_fifo[auth_fifo_pos].connect_until_time;
			memcpy(WFIFOP(map_fd,20), &char_dat[0], sizeof(struct mmo_charstatus));
			WFIFOSET(map_fd, WFIFOW(map_fd,2));

			set_char_online(i, auth_fifo[auth_fifo_pos].char_id, auth_fifo[auth_fifo_pos].account_id);
			//Checks to see if the even share setting of the party must be broken.
			inter_party_logged(char_dat[0].party_id, char_dat[0].account_id);
			auth_fifo_pos++;
			break;

		case 0x67:	// make new
			FIFOSD_CHECK(37);

			if(char_new == 0) //turn character creation on/off [Kevin]
				i = -2;
			else
				i = make_new_char_sql(fd, RFIFOP(fd, 2));

			//if (i < 0) {
			//	WFIFOW(fd, 0) = 0x6e;
			//	WFIFOB(fd, 2) = 0x00;
			//	WFIFOSET(fd, 3);
			//	RFIFOSKIP(fd, 37);
			//	break;
			//}
			//Changed that we can support 'Charname already exists' (-1) amd 'Char creation denied' (-2)
			//And 'You are underaged' (-3) (XD) [Sirius]
			if(i == -1){
                       //already exists
                            WFIFOW(fd, 0) = 0x6e;
                            WFIFOB(fd, 2) = 0x00;
                            WFIFOSET(fd, 3);
                            RFIFOSKIP(fd, 37);
                            break;
			}else if(i == -2){
                        //denied
                            WFIFOW(fd, 0) = 0x6e;
                            WFIFOB(fd, 2) = 0x02;
                            WFIFOSET(fd, 3);
                            RFIFOSKIP(fd, 37);
                            break;
			}else if(i == -3){
                        //underaged XD
                            WFIFOW(fd, 0) = 0x6e;
                            WFIFOB(fd, 2) = 0x01;
                            WFIFOSET(fd, 3);
                            RFIFOSKIP(fd, 37);
                            break;
			}

			WFIFOW(fd, 0) = 0x6d;
			memset(WFIFOP(fd, 2), 0x00, 106);

			mmo_char_fromsql_short(i, char_dat); //Only the short data is needed.
			//mmo_char_fromsql(i, char_dat);
			i = 0;
			WFIFOL(fd, 2) = char_dat[i].char_id;
			WFIFOL(fd,2+4) = char_dat[i].base_exp;
			WFIFOL(fd,2+8) = char_dat[i].zeny;
			WFIFOL(fd,2+12) = char_dat[i].job_exp;
			WFIFOL(fd,2+16) = char_dat[i].job_level;

			WFIFOL(fd,2+28) = char_dat[i].karma;
			WFIFOL(fd,2+32) = char_dat[i].manner;

			WFIFOW(fd,2+40) = 0x30;
			WFIFOW(fd,2+42) = (char_dat[i].hp > 0x7fff) ? 0x7fff : char_dat[i].hp;
			WFIFOW(fd,2+44) = (char_dat[i].max_hp > 0x7fff) ? 0x7fff : char_dat[i].max_hp;
			WFIFOW(fd,2+46) = (char_dat[i].sp > 0x7fff) ? 0x7fff : char_dat[i].sp;
			WFIFOW(fd,2+48) = (char_dat[i].max_sp > 0x7fff) ? 0x7fff : char_dat[i].max_sp;
			WFIFOW(fd,2+50) = DEFAULT_WALK_SPEED; // char_dat[i].speed;
			WFIFOW(fd,2+52) = char_dat[i].class_;
			WFIFOW(fd,2+54) = char_dat[i].hair;

			WFIFOW(fd,2+58) = char_dat[i].base_level;
			WFIFOW(fd,2+60) = char_dat[i].skill_point;

			WFIFOW(fd,2+64) = char_dat[i].shield;
			WFIFOW(fd,2+66) = char_dat[i].head_top;
			WFIFOW(fd,2+68) = char_dat[i].head_mid;
			WFIFOW(fd,2+70) = char_dat[i].hair_color;

			memcpy(WFIFOP(fd,2+74), char_dat[i].name, NAME_LENGTH);

			WFIFOB(fd,2+98) = char_dat[i].str>255?255:char_dat[i].str;
			WFIFOB(fd,2+99) = char_dat[i].agi>255?255:char_dat[i].agi;
			WFIFOB(fd,2+100) = char_dat[i].vit>255?255:char_dat[i].vit;
			WFIFOB(fd,2+101) = char_dat[i].int_>255?255:char_dat[i].int_;
			WFIFOB(fd,2+102) = char_dat[i].dex>255?255:char_dat[i].dex;
			WFIFOB(fd,2+103) = char_dat[i].luk>255?255:char_dat[i].luk;
			WFIFOB(fd,2+104) = char_dat[i].char_num;

			WFIFOSET(fd, 108);
			RFIFOSKIP(fd, 37);

			//to do
			for(ch = 0; ch < 9; ch++) {
				if (sd->found_char[ch] == -1) {
					sd->found_char[ch] = char_dat[i].char_id;
					break;
				}
			}
			break;
		case 0x68: /* delete char */
			FIFOSD_CHECK(46);
			ShowInfo("\033[1;31m Request Char Deletion:\033[0m \033[1;32m%d\033[0m(\033[1;32m%d\033[0m)\n", sd->account_id, RFIFOL(fd, 2));
			memcpy(email, RFIFOP(fd,6), 40);

			/* Check if e-mail is correct */
			if(strcmpi(email, sd->email)){
				if(strcmp("a@a.com", sd->email) == 0){
					if(strcmp("a@a.com", email) == 0 || strcmp("", email) == 0){
						//ignore
					}else{
						//del fail
						WFIFOW(fd, 0) = 0x70;
						WFIFOB(fd, 2) = 0;
						WFIFOSET(fd, 3);
						RFIFOSKIP(fd, 46);
						break;
					}
				}else{
					//del fail
					WFIFOW(fd, 0) = 0x70;
					WFIFOB(fd, 2) = 0;
					WFIFOSET(fd, 3);
					RFIFOSKIP(fd, 46);
					break;
				}
			}

			for(i = 0; i < 9; i++) {
				/* Debug:
				printf("Checking if char to be deleted: %d - %d (%d)\n", sd->found_char[i], RFIFOL(fd, 2), sd->account_id);
				*/
				if (sd->found_char[i] == RFIFOL(fd, 2)) {
					for(ch = i; ch < 9-1; ch++)
						sd->found_char[ch] = sd->found_char[ch+1];
					sd->found_char[8] = -1;
					break;
				}
			}
			/* Such a character does not exist in the account */
			/* If so, you are so screwed. */
			if (i == 9) {
				WFIFOW(fd, 0) = 0x70;
				WFIFOB(fd, 2) = 0;
				WFIFOSET(fd, 3);
				break;
			}

			/* Grab the partner id */
			sprintf(tmp_sql, "SELECT `partner_id` FROM `%s` WHERE `char_id`='%d'",char_db, RFIFOL(fd,2));

			if (mysql_query(&mysql_handle, tmp_sql)) {
				ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
				ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			}

			sql_res = mysql_store_result(&mysql_handle);

			if(sql_res)
			{
				int char_pid=0;
				sql_row = mysql_fetch_row(sql_res);
				if (sql_row)
					char_pid = atoi(sql_row[0]);
				mysql_free_result(sql_res);

				/* Delete character and partner (if any) */
				delete_char_sql(RFIFOL(fd,2), char_pid);
				if (char_pid != 0)
				{	/* If there is partner, tell map server to do divorce */
					WBUFW(buf,0) = 0x2b12;
					WBUFL(buf,2) = RFIFOL(fd,2);
					WBUFL(buf,6) = char_pid;
					mapif_sendall(buf,10);
				}
			}
			/* Char successfully deleted. <- For sure? There could had been an sql db error, what is done then?. [Skotlex] */
			WFIFOW(fd, 0) = 0x6f;
			WFIFOSET(fd, 2);

			RFIFOSKIP(fd, 46);
			break;

		case 0x2af8: // login as map-server
			if (RFIFOREST(fd) < 60)
				return 0;
			WFIFOW(fd, 0) = 0x2af9;
			for(i = 0; i < MAX_MAP_SERVERS; i++) {
				if (server_fd[i] <= 0)
					break;
			}
			if (i == MAX_MAP_SERVERS || strcmp((const char*)RFIFOP(fd,2), userid) || strcmp((const char*)RFIFOP(fd,26), passwd)) {
				WFIFOB(fd,2) = 3;
				WFIFOSET(fd, 3);
			} else {
//				int len;
				WFIFOB(fd,2) = 0;
				WFIFOSET(fd, 3);
				session[fd]->func_parse = parse_frommap;
				server_fd[i] = fd;
				server[i].ip = RFIFOL(fd, 54);
				server[i].port = RFIFOW(fd, 58);
				server[i].users = 0;
				memset(server[i].map, 0, sizeof(server[i].map));
				realloc_fifo(fd, FIFOSIZE_SERVERLINK, FIFOSIZE_SERVERLINK);
				char_mapif_init(fd);
				// send gm acccounts level to map-servers
/* removed by CLOWNISIUS due to isGM
				len = 4;
				WFIFOW(fd,0) = 0x2b15;
				for(i = 0; i < GM_num; i++) {
					WFIFOL(fd,len) = gm_account[i].account_id;
					WFIFOB(fd,len+4) = (unsigned char)gm_account[i].level;
					len += 5;
				}
				WFIFOW(fd,2) = len;
				WFIFOSET(fd,len);*/
			}
			RFIFOSKIP(fd,60);
			break;

		case 0x187:	// Alive?
			if (RFIFOREST(fd) < 6) {
				return 0;
			}
			RFIFOSKIP(fd, 6);
			break;

		case 0x7530:	// Athena info get
			WFIFOW(fd, 0) = 0x7531;
			WFIFOB(fd, 2) = ATHENA_MAJOR_VERSION;
			WFIFOB(fd, 3) = ATHENA_MINOR_VERSION;
			WFIFOB(fd, 4) = ATHENA_REVISION;
			WFIFOB(fd, 5) = ATHENA_RELEASE_FLAG;
			WFIFOB(fd, 6) = ATHENA_OFFICIAL_FLAG;
			WFIFOB(fd, 7) = ATHENA_SERVER_INTER | ATHENA_SERVER_CHAR;
			WFIFOW(fd, 8) = ATHENA_MOD_VERSION;
			WFIFOSET(fd, 10);
			RFIFOSKIP(fd, 2);
			return 0;

		case 0x7532:	// disconnect(default also disconnect)
		default:
			session[fd]->eof = 1;
			return 0;
		}
	}
	RFIFOFLUSH(fd);

	return 0;
}

// Console Command Parser [Wizputer]
int parse_console(char *buf) {
    char *type,*command;

    type = (char *)aMalloc(64);
    command = (char *)aMalloc(64);

    memset(type,0,64);
    memset(command,0,64);

    ShowNotice("Console: %s\n",buf);

    if ( sscanf(buf, "%[^:]:%[^\n]", type , command ) < 2 )
        sscanf(buf,"%[^\n]",type);

    ShowNotice("Type of command: %s || Command: %s \n",type,command);

    if(buf) aFree(buf);
    if(type) aFree(type);
    if(command) aFree(command);

    return 0;
}

// MAP send all
int mapif_sendall(unsigned char *buf, unsigned int len) {
	int i, c;
	int fd;

	c = 0;
	for(i = 0; i < MAX_MAP_SERVERS; i++) {
		if ((fd = server_fd[i]) > 0) { //0 Should not be a valid server_fd [Skotlex]
			if (session[fd] == NULL)
			{	//Could this be the crash's source? [Skotlex]
				ShowError("mapif_sendall: Attempting to write to invalid session %d! Map Server #%d disconnected.\n", fd, i);
				server_fd[i] = -1;
				memset(&server[i], 0, sizeof(struct mmo_map_server));
				continue;
			}
			if (WFIFOSPACE(fd) < len) //Increase buffer size.
             realloc_writefifo(fd, len);
			memcpy(WFIFOP(fd,0), buf, len);
			WFIFOSET(fd,len);
			c++;
		}
	}

	return c;
}

int mapif_sendallwos(int sfd, unsigned char *buf, unsigned int len) {
	int i, c;
	int fd;

	c = 0;
	for(i=0, c=0;i<MAX_MAP_SERVERS;i++){
		if ((fd = server_fd[i]) > 0 && fd != sfd) {
			if (WFIFOSPACE(fd) < len) //Increase buffer size.
             realloc_writefifo(fd, len);
			memcpy(WFIFOP(fd,0), buf, len);
			WFIFOSET(fd, len);
			c++;
		}
	}

	return c;
}

int mapif_send(int fd, unsigned char *buf, unsigned int len) {
	int i;

	if (fd >= 0) {
		for(i = 0; i < MAX_MAP_SERVERS; i++) {
			if (fd == server_fd[i]) {
			if (WFIFOSPACE(fd) < len) //Increase buffer size.
             realloc_writefifo(fd, len);
				memcpy(WFIFOP(fd,0), buf, len);
				WFIFOSET(fd,len);
				return 1;
			}
		}
	}
	return 0;
}

int send_users_tologin(int tid, unsigned int tick, int id, int data) {
	int users = count_users();
	unsigned char buf[16];

	if (login_fd > 0 && session[login_fd]) {
		// send number of user to login server
		WFIFOW(login_fd,0) = 0x2714;
		WFIFOL(login_fd,2) = users;
		WFIFOSET(login_fd,6);
	}
	// send number of players to all map-servers
	WBUFW(buf,0) = 0x2b00;
	WBUFL(buf,2) = users;
	mapif_sendall(buf, 6);

	return 0;
}

int check_connect_login_server(int tid, unsigned int tick, int id, int data) {
	if (login_fd <= 0 || session[login_fd] == NULL) {
		struct char_session_data *sd;
		int i, cc;
		unsigned char buf[16];

		ShowInfo("Attempt to connect to login-server...\n");
		login_fd = make_connection(login_ip, login_port);
		if (login_fd == -1)
		{	//Try again later. [Skotlex]
			login_fd = 0;
			return 0;
		}
		session[login_fd]->func_parse = parse_tologin;
		realloc_fifo(login_fd, FIFOSIZE_SERVERLINK, FIFOSIZE_SERVERLINK);
		WFIFOW(login_fd,0) = 0x2710;
		memset(WFIFOP(login_fd,2), 0, 24);
		memcpy(WFIFOP(login_fd,2), userid, strlen(userid) < 24 ? strlen(userid) : 24);
		memset(WFIFOP(login_fd,26), 0, 24);
		memcpy(WFIFOP(login_fd,26), passwd, strlen(passwd) < 24 ? strlen(passwd) : 24);
		WFIFOL(login_fd,50) = 0;
		WFIFOL(login_fd,54) = char_ip;
		WFIFOL(login_fd,58) = char_port;
		memset(WFIFOP(login_fd,60), 0, 20);
		memcpy(WFIFOP(login_fd,60), server_name, strlen(server_name) < 20 ? strlen(server_name) : 20);
		WFIFOW(login_fd,80) = 0;
		WFIFOW(login_fd,82) = char_maintenance;

		WFIFOW(login_fd,84) = char_new_display; //only display (New) if they want to [Kevin]

		WFIFOSET(login_fd,86);

		//(re)connected to login-server,
		//now wi'll look in sql which player's are ON and set them OFF
		//AND send to all mapservers (if we have one / ..) to kick the players
		//so the bug is fixed, if'ure using more than one charservers (worlds)
		//that the player'S got reejected from server after a 'world' crash^^
		//2b1f AID.L B1

		sprintf(tmp_sql, "SELECT `account_id`, `online` FROM `%s` WHERE `online` = '1'", char_db);
		if(mysql_query(&mysql_handle, tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			return -1;
		}

		sql_res = mysql_store_result(&mysql_handle);
		if(sql_res){
			cc = (int)mysql_num_rows(sql_res);
			ShowStatus("Setting %d Players offline\n", cc);
			while((sql_row = mysql_fetch_row(sql_res))){
				//sql_row[0] == AID
				//tell the loginserver
				WFIFOW(login_fd, 0) = 0x272c; //set off
				WFIFOL(login_fd, 2) = atoi(sql_row[0]); //AID
				WFIFOSET(login_fd, 6);

				//tell map to 'kick' the player (incase of 'on' ..)
				WBUFW(buf, 0) = 0x2b1f;
				WBUFL(buf, 2) = atoi(sql_row[0]);
				WBUFB(buf, 6) = 1;
				mapif_sendall(buf, 7);

				//kick the player if he's on charselect
				for(i = 0; i < fd_max; i++){
					if(session[i] && (sd = (struct char_session_data*)session[i]->session_data)){
						if(sd->account_id == atoi(sql_row[0])){
							session[i]->eof = 1;
							break;
						}
					}
				}

			}
			mysql_free_result(sql_res);
		}else{
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
			return -1;
		}

		//Now Update all players to 'OFFLINE'
		sprintf(tmp_sql, "UPDATE `%s` SET `online` = '0'", char_db);
		if(mysql_query(&mysql_handle, tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
		sprintf(tmp_sql, "UPDATE `%s` SET `online` = '0'", guild_member_db);
		if(mysql_query(&mysql_handle, tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
		sprintf(tmp_sql, "UPDATE `%s` SET `connect_member` = '0'", guild_db);
		if(mysql_query(&mysql_handle, tmp_sql)){
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}

	}
	return 0;
}

//------------------------------------------------
//Invoked 15 seconds after mapif_disconnectplayer in case the map server doesn't
//replies/disconnect the player we tried to kick. [Skotlex]
//------------------------------------------------
static int chardb_waiting_disconnect(int tid, unsigned int tick, int id, int data)
{
	struct online_char_data* character;
	if ((character = numdb_search(online_char_db, id)) != NULL && character->waiting_disconnect)
	{	//Mark it offline due to timeout.
		set_char_offline(character->char_id, character->account_id);
	}
	return 0;
}

//----------------------------------------------------------
// Return numerical value of a switch configuration by [Yor]
// on/off, english, fran軋is, deutsch, espa�ol
//----------------------------------------------------------
int config_switch(const char *str) {
	if (strcmpi(str, "on") == 0 || strcmpi(str, "yes") == 0 || strcmpi(str, "oui") == 0 || strcmpi(str, "ja") == 0 || strcmpi(str, "si") == 0)
		return 1;
	if (strcmpi(str, "off") == 0 || strcmpi(str, "no") == 0 || strcmpi(str, "non") == 0 || strcmpi(str, "nein") == 0)
		return 0;

	return atoi(str);
}

// Lan Support conf reading added by Kashy
int char_lan_config_read(const char *lancfgName){
	char subnetmask[128];
	char line[1024], w1[1024], w2[1024];
	FILE *fp;
	struct hostent * h = NULL;

	if ((fp = fopen(lancfgName, "r")) == NULL) {
		ShowError("file not found: %s\n", lancfgName);
		return 1;
	}

	ShowInfo("Reading file %s...\n", lancfgName);

	while(fgets(line, sizeof(line)-1, fp)){
		if (line[0] == '/' && line[1] == '/')
			continue;

		if (sscanf(line, "%[^:]: %[^\r\n]", w1, w2) != 2)
			continue;

		else if (strcmpi(w1, "lan_map_ip") == 0) {
			h = gethostbyname(w2);
			if (h != NULL) {
				sprintf(lan_map_ip, "%d.%d.%d.%d", (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1], (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
			} else {
				strncpy(lan_map_ip, w2, sizeof(lan_map_ip));
				lan_map_ip[sizeof(lan_map_ip)-1] = 0;
			}
			ShowStatus("set Lan_map_IP : %s\n", lan_map_ip);
		}

		else if (strcmpi(w1, "subnetmask") == 0) {
			unsigned int k0, k1, k2, k3;
			strcpy(subnetmask, w2);
			sscanf(subnetmask, "%d.%d.%d.%d", &k0, &k1, &k2, &k3);
			subnetmaski[0] = k0; subnetmaski[1] = k1; subnetmaski[2] = k2; subnetmaski[3] = k3;
			ShowStatus("set subnetmask : %s\n", w2);
		}
	}
	fclose(fp);

	ShowInfo("Done reading %s.\n", lancfgName);
	return 0;
}

static int char_db_final(void *key,void *data,va_list ap)
{
	if (data) aFree(data);
	return 0;
}

void do_final(void) {
	ShowInfo("Doing final stage...\n");
	//mmo_char_sync();
	//inter_save();
	do_final_itemdb();
	//check SQL save progress.
	//wait until save char complete

	set_all_offline();

	inter_final();

	flush_fifos();

	sprintf(tmp_sql,"DELETE FROM `ragsrvinfo");
	if (mysql_query(&mysql_handle, tmp_sql))
	{
		ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
		ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
	}

	if(gm_account)  {
		aFree(gm_account);
		gm_account = 0;
	}

	if(char_dat)  {
		aFree(char_dat);
		char_dat = 0;
	}

	delete_session(login_fd);
	delete_session(char_fd);
	numdb_final(char_db_, char_db_final);
	numdb_final(online_char_db, char_db_final);

	mysql_close(&mysql_handle);
	mysql_close(&lmysql_handle);

	ShowInfo("ok! all done...\n");
}

void sql_config_read(const char *cfgName){ /* Kalaspuff, to get login_db */
	char line[1024], w1[1024], w2[1024];
	FILE *fp;

	ShowInfo("Reading file %s...\n", cfgName);

	if ((fp = fopen(cfgName, "r")) == NULL) {
		ShowFatalError("file not found: %s\n", cfgName);
		exit(1);
	}

	while(fgets(line, sizeof(line)-1, fp)){
		if(line[0] == '/' && line[1] == '/')
			continue;

		if (sscanf(line, "%[^:]: %[^\r\n]", w1, w2) != 2)
			continue;

		if(strcmpi(w1,"char_db")==0){
			strcpy(char_db,w2);
		}else if(strcmpi(w1,"scdata_db")==0){
			strcpy(scdata_db,w2);
		}else if(strcmpi(w1,"cart_db")==0){
			strcpy(cart_db,w2);
		}else if(strcmpi(w1,"inventory_db")==0){
			strcpy(inventory_db,w2);
		}else if(strcmpi(w1,"charlog_db")==0){
			strcpy(charlog_db,w2);
		}else if(strcmpi(w1,"storage_db")==0){
			strcpy(storage_db,w2);
		}else if(strcmpi(w1,"reg_db")==0){
			strcpy(reg_db,w2);
		}else if(strcmpi(w1,"skill_db")==0){
			strcpy(skill_db,w2);
		}else if(strcmpi(w1,"interlog_db")==0){
			strcpy(interlog_db,w2);
		}else if(strcmpi(w1,"memo_db")==0){
			strcpy(memo_db,w2);
		}else if(strcmpi(w1,"guild_db")==0){
			strcpy(guild_db,w2);
		}else if(strcmpi(w1,"guild_alliance_db")==0){
			strcpy(guild_alliance_db,w2);
		}else if(strcmpi(w1,"guild_castle_db")==0){
			strcpy(guild_castle_db,w2);
		}else if(strcmpi(w1,"guild_expulsion_db")==0){
			strcpy(guild_expulsion_db,w2);
		}else if(strcmpi(w1,"guild_member_db")==0){
			strcpy(guild_member_db,w2);
		}else if(strcmpi(w1,"guild_skill_db")==0){
			strcpy(guild_skill_db,w2);
		}else if(strcmpi(w1,"guild_position_db")==0){
			strcpy(guild_position_db,w2);
		}else if(strcmpi(w1,"guild_storage_db")==0){
			strcpy(guild_storage_db,w2);
		}else if(strcmpi(w1,"party_db")==0){
			strcpy(party_db,w2);
		}else if(strcmpi(w1,"pet_db")==0){
			strcpy(pet_db,w2);
		}else if(strcmpi(w1,"friend_db")==0){
			strcpy(friend_db,w2);
		}else if(strcmpi(w1,"db_path")==0){
			strcpy(db_path,w2);
		//Map server option to use SQL db or not
		}else if(strcmpi(w1,"use_sql_db")==0){ // added for sql item_db read for char server [Valaris]
			db_use_sqldbs = config_switch(w2);
			ShowStatus("Using SQL dbs: %s\n",w2);
		//custom columns for login database
		}else if(strcmpi(w1,"login_db_level")==0){
			strcpy(login_db_level,w2);
		}else if(strcmpi(w1,"login_db_account_id")==0){
			strcpy(login_db_account_id,w2);
		}else if(strcmpi(w1,"lowest_gm_level")==0){
			lowest_gm_level = atoi(w2);
			ShowStatus("set lowest_gm_level : %s\n",w2);
		//support the import command, just like any other config
		}else if(strcmpi(w1,"import")==0){
			sql_config_read(w2);
		}

	}
	fclose(fp);
	ShowInfo("done reading %s.\n", cfgName);
}

int char_config_read(const char *cfgName) {
	struct hostent *h = NULL;
	char line[1024], w1[1024], w2[1024];
	FILE *fp;

	if ((fp = fopen(cfgName, "r")) == NULL) {
		ShowFatalError("Configuration file not found: %s.\n", cfgName);
		exit(1);
	}

	ShowInfo("Reading file %s...\n", cfgName);
	while(fgets(line, sizeof(line)-1, fp)) {
		if (line[0] == '/' && line[1] == '/')
			continue;

		line[sizeof(line)-1] = '\0';
		if (sscanf(line,"%[^:]: %[^\r\n]", w1, w2) != 2)
			continue;

		remove_control_chars((unsigned char *) w1);
		remove_control_chars((unsigned char *) w2);
		if(strcmpi(w1,"timestamp_format")==0) {
			strncpy(timestamp_format, w2, 20);
		} else if(strcmpi(w1,"console_silent")==0){
			msg_silent = 0; //To always allow the next line to show up.
			ShowInfo("Console Silent Setting: %d\n", atoi(w2));
			msg_silent = atoi(w2);
		} else if (strcmpi(w1, "userid") == 0) {
			memcpy(userid, w2, 24);
		} else if (strcmpi(w1, "passwd") == 0) {
			memcpy(passwd, w2, 24);
		} else if (strcmpi(w1, "server_name") == 0) {
			memcpy(server_name, w2, sizeof(server_name));
			server_name[sizeof(server_name) - 1] = '\0';
			ShowStatus("%s server has been initialized\n", w2);
		} else if (strcmpi(w1, "wisp_server_name") == 0) {
			if (strlen(w2) >= 4) {
				memcpy(wisp_server_name, w2, sizeof(wisp_server_name));
				wisp_server_name[sizeof(wisp_server_name) - 1] = '\0';
			}
		} else if (strcmpi(w1, "login_ip") == 0) {
			login_ip_set_ = 1;
			h = gethostbyname (w2);
			if (h != NULL) {
				ShowStatus("Login server IP address : %s -> %d.%d.%d.%d\n", w2, (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1], (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
				sprintf(login_ip_str, "%d.%d.%d.%d", (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1], (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
			} else
				memcpy(login_ip_str,w2,16);
		} else if (strcmpi(w1, "login_port") == 0) {
			login_port=atoi(w2);
		} else if (strcmpi(w1, "char_ip") == 0) {
			char_ip_set_ = 1;
			h = gethostbyname (w2);
			if(h != NULL) {
				ShowStatus("Character server IP address : %s -> %d.%d.%d.%d\n", w2, (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1], (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
				sprintf(char_ip_str, "%d.%d.%d.%d", (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1], (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
			} else
				memcpy(char_ip_str, w2, 16);
		} else if (strcmpi(w1, "bind_ip") == 0) {
			bind_ip_set_ = 1;
			h = gethostbyname (w2);
			if(h != NULL) {
				ShowStatus("Character server binding IP address : %s -> %d.%d.%d.%d\n", w2, (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1], (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
				sprintf(bind_ip_str, "%d.%d.%d.%d", (unsigned char)h->h_addr[0], (unsigned char)h->h_addr[1], (unsigned char)h->h_addr[2], (unsigned char)h->h_addr[3]);
			} else
				memcpy(bind_ip_str, w2, 16);
		} else if (strcmpi(w1, "char_port") == 0) {
			char_port = atoi(w2);
		} else if (strcmpi(w1, "char_maintenance") == 0) {
			char_maintenance = atoi(w2);
		} else if (strcmpi(w1, "char_new")==0){
			char_new = atoi(w2);
		} else if (strcmpi(w1, "char_new_display")==0){
			char_new_display = atoi(w2);
		} else if (strcmpi(w1, "max_connect_user") == 0) {
			max_connect_user = atoi(w2);
			if (max_connect_user < 0)
				max_connect_user = 0; // unlimited online players
		} else if(strcmpi(w1, "gm_allow_level") == 0) {
			gm_allow_level = atoi(w2);
			if(gm_allow_level < 0)
				gm_allow_level = 99;
		} else if (strcmpi(w1, "check_ip_flag") == 0) {
			check_ip_flag = config_switch(w2);
		} else if (strcmpi(w1, "online_check") == 0) {
			online_check = config_switch(w2);
		} else if (strcmpi(w1, "autosave_time") == 0) {
			autosave_interval = atoi(w2)*1000;
			if (autosave_interval <= 0)
				autosave_interval = DEFAULT_AUTOSAVE_INTERVAL;
		} else if (strcmpi(w1, "save_log") == 0) {
			save_log = config_switch(w2);
		} else if (strcmpi(w1, "start_point") == 0) {
			char map[MAP_NAME_LENGTH];
			int x, y;
			if (sscanf(w2,"%15[^,],%d,%d", map, &x, &y) < 3)
				continue;
			if (strstr(map, ".gat") != NULL) { // Verify at least if '.gat' is in the map name
				memcpy(start_point.map, map, MAP_NAME_LENGTH);
				start_point.x = x;
				start_point.y = y;
			}
		} else if (strcmpi(w1, "start_zeny") == 0) {
			start_zeny = atoi(w2);
			if (start_zeny < 0)
				start_zeny = 0;
		} else if (strcmpi(w1, "start_weapon") == 0) {
			start_weapon = atoi(w2);
			if (start_weapon < 0)
				start_weapon = 0;
		} else if (strcmpi(w1, "start_armor") == 0) {
			start_armor = atoi(w2);
			if (start_armor < 0)
				start_armor = 0;
		} else if(strcmpi(w1,"log_char")==0){		//log char or not [devil]
			log_char = atoi(w2);
		} else if (strcmpi(w1, "unknown_char_name") == 0) {
			strcpy(unknown_char_name, w2);
			unknown_char_name[NAME_LENGTH-1] = 0;
		} else if (strcmpi(w1, "name_ignoring_case") == 0) {
			name_ignoring_case = config_switch(w2);
		} else if (strcmpi(w1, "char_name_option") == 0) {
			char_name_option = atoi(w2);
		} else if (strcmpi(w1, "char_name_letters") == 0) {
			strcpy(char_name_letters, w2);
		} else if (strcmpi(w1, "check_ip_flag") == 0) {
			check_ip_flag = config_switch(w2);
		} else if (strcmpi(w1, "chars_per_account") == 0) { //maxchars per account [Sirius]
			char_per_account = atoi(w2);
		} else if (strcmpi(w1, "console") == 0) {
			if(strcmpi(w2,"on") == 0 || strcmpi(w2,"yes") == 0 )
				console = 1;
		} else if (strcmpi(w1, "import") == 0) {
			char_config_read(w2);
		}
	}
	fclose(fp);

	ShowInfo("Done reading %s.\n", cfgName);

	return 0;
}

void set_server_type(void)
{
	SERVER_TYPE = ATHENA_SERVER_CHAR;
}

static int online_data_cleanup_sub(void *key, void *data, va_list ap)
{
	struct online_char_data *character= (struct online_char_data*)data;
	if (character->server == -2) //Unknown server.. set them offline
		set_char_offline(character->char_id, character->account_id);
	if (character->server < 0)
	{	//Free data from players that have not been online for a while.
		numdb_erase(online_char_db, character->account_id);
		aFree(data);
	}
	return 0;
}

static int online_data_cleanup(int tid, unsigned int tick, int id, int data)
{
	db_foreach(online_char_db, online_data_cleanup_sub);
	return 0;
}

int do_init(int argc, char **argv){
	int i;

	for(i = 0; i < MAX_MAP_SERVERS; i++) {
		memset(&server[i], 0, sizeof(struct mmo_map_server));
		server_fd[i] = -1;
	}

	char_config_read((argc < 2) ? CHAR_CONF_NAME : argv[1]);
	char_lan_config_read((argc > 1) ? argv[1] : LAN_CONF_NAME);
	sql_config_read(SQL_CONF_NAME);

	ShowInfo("Finished reading the char-server configuration.\n");

	inter_init((argc > 2) ? argv[2] : inter_cfgName); // inter server ﾃﾊｱ篳ｭ
	ShowInfo("Finished reading the inter-server configuration.\n");

	//Read ItemDB
	do_init_itemdb();

	ShowInfo("Initializing char server.\n");
	online_char_db = numdb_init();
	mmo_char_sql_init();
	ShowInfo("char server initialized.\n");

//	ShowDebug("set parser -> parse_char()...\n");
	set_defaultparse(parse_char);

//	ShowDebug("set terminate function -> do_final().....\n");

        if ((naddr_ != 0) && (login_ip_set_ == 0 || char_ip_set_ == 0)) {
          // The char server should know what IP address it is running on
          //   - MouseJstr
          int localaddr = ntohl(addr_[0]);
          unsigned char *ptr = (unsigned char *) &localaddr;
          char buf[16];
          sprintf(buf, "%d.%d.%d.%d", ptr[0], ptr[1], ptr[2], ptr[3]);
          if (naddr_ != 1)
            ShowStatus("Multiple interfaces detected..  using %s as our IP address\n", buf);
          else
            ShowStatus("Defaulting to %s as our IP address\n", buf);
          if (login_ip_set_ == 0)
          	strcpy(login_ip_str, buf);
          if (char_ip_set_ == 0)
          	strcpy(char_ip_str, buf);

          if (ptr[0] == 192 && ptr[1] == 168)
            ShowWarning("Firewall detected.. edit lan_support.conf and char_athena.conf\n");
        }

	login_ip = inet_addr(login_ip_str);
	char_ip = inet_addr(char_ip_str);

	ShowInfo("open port %d.....\n",char_port);
	//char_fd = make_listen_port(char_port);
	if (bind_ip_set_)
		char_fd = make_listen_bind(inet_addr(bind_ip_str),char_port);
	else
		char_fd = make_listen_bind(INADDR_ANY,char_port);

	add_timer_func_list(check_connect_login_server, "check_connect_login_server");
	add_timer_func_list(send_users_tologin, "send_users_tologin");
	add_timer_func_list(chardb_waiting_disconnect, "chardb_waiting_disconnect");

	add_timer_func_list(online_data_cleanup, "online_data_cleanup");
	add_timer_interval(gettick() + 600*1000, online_data_cleanup, 0, 0, 600 * 1000);

	// send ALIVE PING to login server.
	add_timer_interval(gettick() + 10, check_connect_login_server, 0, 0, 10 * 1000);
	// send USER COUNT PING to login server.
	add_timer_interval(gettick() + 10, send_users_tologin, 0, 0, 5 * 1000);

	if ( console ) {
	    set_defaultconsoleparse(parse_console);
	   	start_console();
	}

	//Cleaning the tables for NULL entrys @ startup [Sirius]
         //Chardb clean
        	ShowInfo("Cleaning the '%s' table...", char_db);
         sprintf(tmp_sql,"DELETE FROM `%s` WHERE `account_id` = '0'", char_db);
         if(mysql_query(&mysql_handle, tmp_sql)){
           //error on clean
            printf(" fail.\n");
         }else{
            printf(" done.\n");
         }

         //guilddb clean
         ShowInfo("Cleaning the '%s' table...", guild_db);
         sprintf(tmp_sql,"DELETE FROM `%s` WHERE `guild_lv` = '0' AND `max_member` = '0' AND `exp` = '0' AND `next_exp` = '0' AND `average_lv` = '0'", guild_db);
         if(mysql_query(&mysql_handle, tmp_sql)){
          //error on clean
            printf(" fail.\n");
         }else{
            printf(" done.\n");
         }

         //guildmemberdb clean
         ShowInfo("Cleaning the '%s' table...", guild_member_db);
         sprintf(tmp_sql,"DELETE FROM `%s` WHERE `guild_id` = '0' AND `account_id` = '0' AND `char_id` = '0'", guild_member_db);
         if(mysql_query(&mysql_handle, tmp_sql)){
          //error on clean
            printf(" fail.\n");
         }else{
            printf(" done.\n");
         }



	ShowInfo("End of char server initilization function.\n");
	ShowStatus("The char-server is \033[1;32mready\033[0m (Server is listening on the port %d).\n\n", char_port);
	return 0;
}

#undef mysql_query

int debug_mysql_query(char *file, int line, void *mysql, const char *q) {
#ifdef TWILIGHT
        ShowDebug("sql: %s:%d# %s\n", file, line, q);
#endif
        return mysql_query((MYSQL *) mysql, q);
}

int char_child(int parent_id, int child_id) {
		int tmp_id = 0;
		sprintf (tmp_sql, "SELECT `child` FROM `%s` WHERE `char_id` = '%d'", char_db, parent_id);
		if (mysql_query (&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
		sql_res = mysql_store_result (&mysql_handle);
		sql_row = sql_res?mysql_fetch_row (sql_res):NULL;
		if (sql_row)
			tmp_id = atoi (sql_row[0]);
		else
			ShowError("CHAR: child Failed!\n");
		if (sql_res) mysql_free_result (sql_res);
		if ( tmp_id == child_id )
			return 1;
		else
			return 0;
}

int char_married(int pl1,int pl2) {
		int tmp_id = 0;
		sprintf (tmp_sql, "SELECT `partner_id` FROM `%s` WHERE `char_id` = '%d'", char_db, pl1);
		if (mysql_query (&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
		sql_res = mysql_store_result (&mysql_handle);
		sql_row = sql_res?mysql_fetch_row (sql_res):NULL;
		if (sql_row)
			tmp_id = atoi (sql_row[0]);
		else
			ShowError("CHAR: married Failed!\n");
		if (sql_res) mysql_free_result (sql_res);
		if ( tmp_id == pl2 )
			return 1;
		else
			return 0;
}

int char_nick2id (char *name) {
		int char_id = 0;
		sprintf (tmp_sql, "SELECT `char_id` FROM `%s` WHERE `name` = '%s'", char_db, name);
		if (mysql_query (&mysql_handle, tmp_sql)) {
			ShowSQL("DB error - %s\n",mysql_error(&mysql_handle));
			ShowDebug("at %s:%d - %s\n", __FILE__,__LINE__,tmp_sql);
		}
		sql_res = mysql_store_result (&mysql_handle);
		sql_row = sql_res?mysql_fetch_row(sql_res):NULL;
		if (sql_row)
			char_id = atoi (sql_row[0]);
		else
			ShowError ("CHAR: nick2id Failed!\n");
		if (sql_res) mysql_free_result (sql_res);
		return char_id;
}

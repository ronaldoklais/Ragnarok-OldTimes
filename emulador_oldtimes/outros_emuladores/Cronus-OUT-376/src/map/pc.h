// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef _PC_H_
#define _PC_H_

#include "map.h"
#include "unit.h"

#define OPTION_MASK 0xd7b8
#define CART_MASK 0x788

//Update this max as necessary. 53 is the value needed for Super Baby currently
#define MAX_SKILL_TREE 53

enum {
	W_FIST,	//Bare hands
	W_DAGGER,	//1
	W_1HSWORD,	//2
	W_2HSWORD,	//3
	W_1HSPEAR,	//4
	W_2HSPEAR,	//5
	W_1HAXE,	//6
	W_2HAXE,	//7
	W_MACE,	//8
	W_UNKNOWN,	//View 9 seems unused anywhere
	W_STAFF,	//10
	W_BOW,	//11
	W_KNUCKLE,	//12	
	W_MUSICAL,	//13
	W_WHIP,	//14
	W_BOOK,	//15
	W_KATAR,	//16
	W_REVOLVER,	//17
	W_RIFLE,	//18
	W_SHOTGUN,	//19
	W_GATLING,	//20
	W_GRENADE,	//21
	W_HUUMA,	//22
	MAX_WEAPON_TYPE
} weapon_type;

#define pc_setdead(sd) ((sd)->state.dead_sit = (sd)->vd.dead_sit = 1)
#define pc_setsit(sd) ((sd)->state.dead_sit = (sd)->vd.dead_sit = 2)
#define pc_isdead(sd) ((sd)->state.dead_sit == 1)
#define pc_issit(sd) ((sd)->vd.dead_sit == 2)
#define pc_setdir(sd,b,h) ((sd)->ud.dir = (b) ,(sd)->head_dir = (h) )
#define pc_setchatid(sd,n) ((sd)->chatID = n)
#define pc_ishiding(sd) ((sd)->sc.option&(OPTION_HIDE|OPTION_CLOAK|OPTION_CHASEWALK))
#define pc_iscloaking(sd) (!((sd)->sc.option&OPTION_CHASEWALK) && ((sd)->sc.option&OPTION_CLOAK))
#define pc_ischasewalk(sd) ((sd)->sc.option&OPTION_CHASEWALK)
#define pc_iscarton(sd) ((sd)->sc.option&CART_MASK)
#define pc_isfalcon(sd) ((sd)->sc.option&OPTION_FALCON)
#define pc_isriding(sd) ((sd)->sc.option&OPTION_RIDING)
#define pc_isinvisible(sd) ((sd)->sc.option&OPTION_INVISIBLE)
#define pc_is50overweight(sd) (sd->weight*2 >= sd->max_weight) 
#define pc_is90overweight(sd) (sd->weight*10 >= sd->max_weight*9)
#define pc_maxparameter(sd) ((sd->class_&JOBL_BABY) ? battle_config.max_baby_parameter : battle_config.max_parameter)

#define pc_stop_attack(sd) { if (sd->ud.attacktimer!=-1) { unit_stop_attack(&sd->bl); sd->ud.target = 0; } }
#define pc_stop_walking(sd, type) { if (sd->ud.walktimer!=-1) unit_stop_walking(&sd->bl, type); }

//Checks if the given class value corresponds to a player class. [Skotlex]
#define pcdb_checkid(class_) ((class_ >= JOB_NOVICE && class_ <= JOB_XMAS) || (class_ >= JOB_NOVICE_HIGH && class_ <= JOB_SOUL_LINKER))

int pc_isGM(struct map_session_data *sd);
int pc_iskiller(struct map_session_data *src, struct map_session_data *target); // [MouseJstr]
int pc_getrefinebonus(int lv,int type);
int pc_can_give_items(int level); //[Lupus]

int pc_setrestartvalue(struct map_session_data *sd,int type);
int pc_makesavestatus(struct map_session_data *);
int pc_setnewpc(struct map_session_data*,int,int,int,unsigned int,int,int);
int pc_authok(struct map_session_data*, int, time_t, struct mmo_charstatus *);
int pc_authfail(struct map_session_data *);
int pc_reg_received(struct map_session_data *sd);

int pc_isequip(struct map_session_data *sd,int n);
int pc_equippoint(struct map_session_data *sd,int n);

int pc_checkskill(struct map_session_data *sd,int skill_id);
int pc_checkallowskill(struct map_session_data *sd);
int pc_checkequip(struct map_session_data *sd,int pos);

int pc_calc_skilltree(struct map_session_data *sd);
int pc_calc_skilltree_normalize_job(struct map_session_data *sd);
int pc_clean_skilltree(struct map_session_data *sd);

int pc_checkoverhp(struct map_session_data*);
int pc_checkoversp(struct map_session_data*);

int pc_setpos(struct map_session_data*,unsigned short,int,int,int);
int pc_setsavepoint(struct map_session_data*,short,int,int);
int pc_randomwarp(struct map_session_data *sd,int type);
int pc_memo(struct map_session_data *sd,int i);
int pc_remove_map(struct map_session_data *sd,int clrtype);

int pc_checkadditem(struct map_session_data*,int,int);
int pc_inventoryblank(struct map_session_data*);
int pc_search_inventory(struct map_session_data *sd,int item_id);
int pc_payzeny(struct map_session_data*,int);
int pc_additem(struct map_session_data*,struct item*,int);
int pc_getzeny(struct map_session_data*,int);
int pc_delitem(struct map_session_data*,int,int,int);
int pc_checkitem(struct map_session_data*);

int pc_cart_additem(struct map_session_data *sd,struct item *item_data,int amount);
int pc_cart_delitem(struct map_session_data *sd,int n,int amount,int type);
int pc_putitemtocart(struct map_session_data *sd,int idx,int amount);
int pc_getitemfromcart(struct map_session_data *sd,int idx,int amount);
int pc_cartitem_amount(struct map_session_data *sd,int idx,int amount);

int pc_takeitem(struct map_session_data*,struct flooritem_data*);
int pc_dropitem(struct map_session_data*,int,int);

int pc_checkweighticon(struct map_session_data *sd);

int pc_bonus(struct map_session_data*,int,int);
int pc_bonus2(struct map_session_data *sd,int,int,int);
int pc_bonus3(struct map_session_data *sd,int,int,int,int);
int pc_bonus4(struct map_session_data *sd,int,int,int,int,int);
int pc_skill(struct map_session_data*,int,int,int);

int pc_insert_card(struct map_session_data *sd,int idx_card,int idx_equip);

int pc_steal_item(struct map_session_data *sd,struct block_list *bl);
int pc_steal_coin(struct map_session_data *sd,struct block_list *bl);

int pc_modifybuyvalue(struct map_session_data*,int);
int pc_modifysellvalue(struct map_session_data*,int);

int pc_follow(struct map_session_data*, int); // [MouseJstr]
int pc_stop_following(struct map_session_data*);


unsigned int pc_maxbaselv(struct map_session_data *sd);
unsigned int pc_maxjoblv(struct map_session_data *sd);
int pc_checkbaselevelup(struct map_session_data *sd);
int pc_checkjoblevelup(struct map_session_data *sd);
int pc_gainexp(struct map_session_data*,unsigned int,unsigned int);
unsigned int pc_nextbaseexp(struct map_session_data *);
unsigned int pc_nextjobexp(struct map_session_data *);
int pc_need_status_point(struct map_session_data *,int);
int pc_statusup(struct map_session_data*,int);
int pc_statusup2(struct map_session_data*,int,int);
int pc_skillup(struct map_session_data*,int);
int pc_allskillup(struct map_session_data*);
int pc_resetlvl(struct map_session_data*,int type);
int pc_resetstate(struct map_session_data*);
int pc_resetskill(struct map_session_data*, int);
int pc_resetfeel(struct map_session_data*);
int pc_equipitem(struct map_session_data*,int,int);
int pc_unequipitem(struct map_session_data*,int,int);
int pc_checkitem(struct map_session_data*);
int pc_useitem(struct map_session_data*,int);

int pc_damage_sp(struct map_session_data *, int, int);
int pc_damage(struct block_list *,struct map_session_data*,int);
int pc_heal(struct map_session_data *,int,int);
int pc_itemheal(struct map_session_data *sd,int hp,int sp);
int pc_percentheal(struct map_session_data *sd,int,int);
int pc_jobchange(struct map_session_data *,int, int);
int pc_setoption(struct map_session_data *,int);
int pc_setcart(struct map_session_data *sd,int type);
int pc_setfalcon(struct map_session_data *sd);
int pc_setriding(struct map_session_data *sd);
int pc_changelook(struct map_session_data *,int,int);
int pc_equiplookall(struct map_session_data *sd);

int pc_readparam(struct map_session_data*,int);
int pc_setparam(struct map_session_data*,int,int);
int pc_readreg(struct map_session_data*,int);
int pc_setreg(struct map_session_data*,int,int);
char *pc_readregstr(struct map_session_data *sd,int reg);
int pc_setregstr(struct map_session_data *sd,int reg,char *str);

#define pc_readglobalreg(sd,reg) pc_readregistry(sd,reg,3)
#define pc_setglobalreg(sd,reg,val) pc_setregistry(sd,reg,val,3)
#define pc_readglobalreg_str(sd,reg) pc_readregistry_str(sd,reg,3)
#define pc_setglobalreg_str(sd,reg,val) pc_setregistry_str(sd,reg,val,3)
#define pc_readaccountreg(sd,reg) pc_readregistry(sd,reg,2)
#define pc_setaccountreg(sd,reg,val) pc_setregistry(sd,reg,val,2)
#define pc_readaccountregstr(sd,reg) pc_readregistry_str(sd,reg,2)
#define pc_setaccountregstr(sd,reg,val) pc_setregistry_str(sd,reg,val,2)
#define pc_readaccountreg2(sd,reg) pc_readregistry(sd,reg,1)
#define pc_setaccountreg2(sd,reg,val) pc_setregistry(sd,reg,val,1)
#define pc_readaccountreg2str(sd,reg) pc_readregistry_str(sd,reg,1)
#define pc_setaccountreg2str(sd,reg,val) pc_setregistry_str(sd,reg,val,1)
int pc_readregistry(struct map_session_data*,char*,int);
int pc_setregistry(struct map_session_data*,char*,int,int);
char *pc_readregistry_str(struct map_session_data*,char*,int);
int pc_setregistry_str(struct map_session_data*,char*,char*,int);

int pc_addeventtimer(struct map_session_data *sd,int tick,const char *name);
int pc_deleventtimer(struct map_session_data *sd,const char *name);
int pc_cleareventtimer(struct map_session_data *sd);
int pc_addeventtimercount(struct map_session_data *sd,const char *name,int tick);

int pc_calc_pvprank(struct map_session_data *sd);
int pc_calc_pvprank_timer(int tid,unsigned int tick,int id,int data);

int pc_ismarried(struct map_session_data *sd);
int pc_marriage(struct map_session_data *sd,struct map_session_data *dstsd);
int pc_divorce(struct map_session_data *sd);
int pc_adoption(struct map_session_data *sd,struct map_session_data *dstsd,struct map_session_data *jasd);
struct map_session_data *pc_get_partner(struct map_session_data *sd);
struct map_session_data *pc_get_father(struct map_session_data *sd);
struct map_session_data *pc_get_mother(struct map_session_data *sd);
struct map_session_data *pc_get_child(struct map_session_data *sd);

int pc_set_gm_level(int account_id, int level);
void pc_setstand(struct map_session_data *sd);
int pc_candrop(struct map_session_data *sd,int item_id);

int pc_jobid2mapid(unsigned short b_class);	// Skotlex
int pc_mapid2jobid(unsigned short class_, int sex);	// Skotlex

char * job_name(int class_);

struct skill_tree_entry {
	short id;
	unsigned char max;
	unsigned char joblv;
	struct {
		short id;
		unsigned char lv;
	} need[5];
}; // Celest
extern struct skill_tree_entry skill_tree[MAX_PC_CLASS][MAX_SKILL_TREE];

int pc_read_gm_account(int fd);
int pc_setinvincibletimer(struct map_session_data *sd,int);
int pc_delinvincibletimer(struct map_session_data *sd);
int pc_addspiritball(struct map_session_data *sd,int,int);
int pc_delspiritball(struct map_session_data *sd,int,int);
void pc_addfame(struct map_session_data *sd,int count);
int pc_istop10fame(int char_id, int job);
int pc_eventtimer(int tid,unsigned int tick,int id,int data); // for npc_dequeue

extern struct fame_list smith_fame_list[MAX_FAME_LIST];
extern struct fame_list chemist_fame_list[MAX_FAME_LIST];
extern struct fame_list taekwon_fame_list[MAX_FAME_LIST];

int pc_readdb(void);
int do_init_pc(void);
void do_final_pc(void);

enum {ADDITEM_EXIST,ADDITEM_NEW,ADDITEM_OVERAMOUNT};

// timer for night.day
extern int day_timer_tid;
extern int night_timer_tid;
int map_day_timer(int,unsigned int,int,int); // by [yor]
int map_night_timer(int,unsigned int,int,int); // by [yor]

int pc_read_motd(void); // [Valaris]
int pc_disguise(struct map_session_data *sd, int class_);
#endif

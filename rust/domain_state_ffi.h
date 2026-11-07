#ifndef A2FO_RUST_DOMAIN_STATE_FFI_H
#define A2FO_RUST_DOMAIN_STATE_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const uint8_t *data;
    size_t size;
} A2foRsBytes;
typedef struct {
    A2foRsBytes id;
    uint32_t level;
    double next_xp;
    A2foRsBytes next_class;
} A2foRsRank;
typedef struct {
    int32_t status;
    A2foRsBytes class_id;
    uint32_t level;
    uint8_t has_next;
    double next_xp;
} A2foRsResolution;
typedef struct {
    uint8_t enabled;
    uint32_t level;
    double pool;
    double multiplier;
} A2foRsPlayer;
typedef struct {
    uint64_t id;
    uint32_t player;
    A2foRsBytes base_class, current_class;
    uint8_t native_capable, completed, live;
} A2foRsUnit;
typedef struct {
    uint32_t id;
    A2foRsPlayer state;
} A2foRsPlayerRecord;
typedef struct {
    int32_t status;
    uint8_t consumed;
    uint32_t levels_gained;
    A2foRsPlayer state;
    double threshold;
    uint8_t commit;
} A2foRsProgress;
typedef struct {
    void *context;
    uint8_t (*rank_lookup)(void *, A2foRsBytes, A2foRsRank *);
} A2foRsCatalogOps;
typedef struct {
    void *context;
    uint8_t (*get_player)(void *, uint32_t, A2foRsPlayer *);
    uint8_t (*get_unit)(void *, uint64_t, A2foRsUnit *);
    uint8_t (*next_unit)(void *, uint64_t *, A2foRsUnit *);
    void (*erase_unit)(void *, uint64_t);
    void (*erase_player)(void *, uint32_t);
    void (*erase_cache)(void *, uint32_t);
    uint8_t (*cache_get)(void *, uint32_t, uint32_t, double *);
    void (*cache_set)(void *, uint32_t, uint32_t, double);
    A2foRsResolution (*resolve)(void *, A2foRsBytes, uint32_t, A2foRsBytes);
    uint32_t (*native_level)(void *, A2foRsBytes);
    void (*weighted_reset)(void *);
    uint8_t (*weighted_add)(void *, double, double);
    uint8_t (*weighted_result)(void *, double *);
} A2foRsXpOps;
uint8_t a2fo_rs_xp_rank_valid(A2foRsRank rank);
A2foRsResolution a2fo_rs_xp_resolve(A2foRsCatalogOps ops, A2foRsBytes base,
                                    uint32_t level, A2foRsBytes current);
uint8_t a2fo_rs_xp_configure_valid(double multiplier);
void a2fo_rs_xp_reset(A2foRsXpOps ops, uint32_t player);
uint8_t a2fo_rs_xp_remove_allowed(A2foRsXpOps ops, uint64_t id);
int32_t a2fo_rs_xp_transfer_allowed(A2foRsXpOps ops, uint64_t id);
int32_t a2fo_rs_xp_upsert_valid(A2foRsXpOps ops, A2foRsUnit unit);
A2foRsResolution a2fo_rs_xp_target(A2foRsXpOps ops, uint64_t id);
double a2fo_rs_xp_threshold(A2foRsXpOps ops, uint32_t player, uint32_t level,
                            int32_t *status);
A2foRsProgress a2fo_rs_xp_process(A2foRsXpOps ops, uint32_t player, double xp,
                                  uint8_t award);
uint8_t a2fo_rs_xp_state_valid(A2foRsPlayer state);
size_t a2fo_rs_xp_snapshot(const A2foRsPlayerRecord *records, size_t count,
                           uint8_t *output, size_t capacity);
uint8_t a2fo_rs_xp_restore(const uint8_t *bytes, size_t size,
                           A2foRsPlayerRecord *records, size_t capacity, size_t *count);
uint8_t a2fo_rs_xp_restore_one(uint32_t player, const uint8_t *bytes, size_t size,
                               A2foRsPlayerRecord *record);
typedef struct {
    uint32_t row, ordinal;
    A2foRsBytes odf;
    uint64_t member;
} A2foRsSqSlot;
typedef struct {
    uint32_t row, count;
    A2foRsBytes odf;
} A2foRsSqRow;
typedef struct {
    uint64_t id;
    uint32_t team;
    A2foRsBytes odf_key;
} A2foRsSqCraft;
typedef struct {
    uint64_t yard;
    uint32_t team;
    uint8_t compatible, accepted;
    const uint64_t *serviced;
    size_t serviced_count;
} A2foRsSqRepair;
typedef struct {
    uint64_t ticket, squad, yard;
    uint32_t team, slot;
    A2foRsBytes odf;
} A2foRsSqReplacement;
typedef struct {
    uint64_t id;
    uint32_t team;
    size_t slot_count;
    uint8_t reinforce;
    uint8_t has_replacement;
    A2foRsSqReplacement replacement;
} A2foRsSqView;
typedef struct {
    uint64_t squad, representative;
    uint32_t live, maximum;
} A2foRsSqTile;
typedef struct {
    int32_t code;
    uint64_t id;
    uint32_t slot;
    uint8_t retire;
} A2foRsSqPlan;
typedef struct {
    void *context;
    uint8_t (*get_squad)(void *, uint64_t, A2foRsSqView *);
    uint8_t (*next_squad)(void *, uint64_t *, A2foRsSqView *);
    uint8_t (*get_slot)(void *, uint64_t, size_t, A2foRsSqSlot *);
    uint64_t (*member_squad)(void *, uint64_t);
    uint8_t (*launching)(void *, uint64_t);
    void (*emit_tile)(void *, A2foRsSqTile);
    void (*emit_member)(void *, uint64_t);
} A2foRsSqOps;
typedef struct {
    void *context;
    uint8_t (*get_squad)(void *, size_t, A2foRsSqView *);
    uint8_t (*get_row)(void *, size_t, size_t, A2foRsSqRow *);
    uint8_t (*get_slot)(void *, size_t, size_t, A2foRsSqSlot *);
    uint8_t (*validate_definition)(void *, size_t);
} A2foRsSqSavedOps;
typedef uint8_t (*A2foRsSqSlotAt)(void *, size_t, A2foRsSqSlot *);
uint32_t a2fo_rs_sq_live_at(void *context, A2foRsSqSlotAt slot_at, size_t count);
uint64_t a2fo_rs_sq_representative_at(void *context, A2foRsSqSlotAt slot_at,
                                      size_t count);
uint32_t a2fo_rs_sq_live(const A2foRsSqSlot *slots, size_t count);
uint64_t a2fo_rs_sq_representative(const A2foRsSqSlot *slots, size_t count);
size_t a2fo_rs_sq_badge(uint32_t live, size_t maximum, uint8_t *output,
                        size_t capacity);
size_t a2fo_rs_sq_expand(const A2foRsSqRow *rows, size_t row_count,
                         A2foRsSqSlot *output, size_t capacity);
uint8_t a2fo_rs_sq_reserved_yard(A2foRsSqOps ops, uint64_t yard);
size_t a2fo_rs_sq_snapshot_ids(A2foRsSqOps ops, uint64_t *ids, size_t capacity);
A2foRsSqPlan a2fo_rs_sq_create(A2foRsSqOps ops, uint64_t next_id, size_t total,
                               uint32_t team, const A2foRsSqSlot *slots,
                               size_t slot_count, const A2foRsSqCraft *crafts,
                               size_t craft_count, uint8_t launching);
A2foRsSqPlan a2fo_rs_sq_add_launching(A2foRsSqOps ops, uint64_t id, size_t slot,
                                      A2foRsSqCraft craft);
A2foRsSqPlan a2fo_rs_sq_finish_launching(A2foRsSqOps ops, uint64_t id);
A2foRsSqPlan a2fo_rs_sq_remove(A2foRsSqOps ops, uint64_t member, uint32_t new_owner,
                               uint8_t ownership_change);
A2foRsSqPlan a2fo_rs_sq_reserve(A2foRsSqOps ops, uint64_t id, uint64_t next_ticket,
                                A2foRsSqRepair repair);
A2foRsSqPlan a2fo_rs_sq_complete(A2foRsSqOps ops, uint64_t ticket, A2foRsSqCraft craft,
                                 A2foRsSqRepair repair);
A2foRsSqPlan a2fo_rs_sq_cancel(A2foRsSqOps ops, uint64_t ticket);
size_t a2fo_rs_sq_cancel_yard(A2foRsSqOps ops, uint64_t yard, uint64_t *tickets,
                              size_t capacity);
int32_t a2fo_rs_sq_select(A2foRsSqOps ops, const A2foRsSqCraft *selected, size_t count,
                          uint32_t team, size_t physical_limit);
int32_t a2fo_rs_sq_restore_check(A2foRsSqSavedOps ops, uint32_t version,
                                 uint64_t next_squad, uint64_t next_ticket,
                                 size_t count);
#ifdef __cplusplus
}
#endif
#endif

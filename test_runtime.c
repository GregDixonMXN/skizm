/*
 * test_runtime.c - Test the Runtime System
 * 
 * This simulates what the compiler would generate from:
 * 
 *     class Player
 *         var health
 *         var name
 *         
 *         method take_damage: amount do
 *             self.health = self.health - amount
 *             (self.health <= 0) do
 *                 self.die
 *             end
 *         end
 *         
 *         method die do
 *             "Game Over" print
 *         end
 *     end
 *     
 *     method main do
 *         var hero = Player new
 *         hero.health = 100
 *         hero.name = "Roguemaster"
 *         
 *         hero take_damage: 30
 *         hero.health print    -- Should print 70
 *         
 *         hero take_damage: 80
 *         -- Should trigger die
 *     end
 */

#include "runtime.h"
#include <stdio.h>

/* =========================================================================
 * Compiler-Generated: Class Player
 * ========================================================================= */

/* Field indices (compiler assigns these) */
#define PLAYER_FIELD_HEALTH 0
#define PLAYER_FIELD_NAME   1
#define PLAYER_FIELD_COUNT  2

/* Class ID (assigned at runtime registration) */
static ClassID CLASS_PLAYER;

/* Selectors for Player methods */
static SelectorID SEL_TAKE_DAMAGE;
static SelectorID SEL_DIE;

/* Method: take_damage: */
static Object *player_take_damage(Object *self, Object **args, int argc) {
    if (argc < 1) return obj_nil();
    
    /* self.health = self.health - amount */
    Object *health = obj_get_field(self, PLAYER_FIELD_HEALTH);
    Object *amount = args[0];
    Object *new_health = send1(health, SEL_MINUS, amount);
    obj_set_field(self, PLAYER_FIELD_HEALTH, new_health);
    
    /* (self.health <= 0) do: self.die end */
    Object *zero = obj_int(0);
    Object *is_dead = send1(new_health, SEL_LE, zero);
    if (obj_is_truthy(is_dead)) {
        send0(self, SEL_DIE);
    }
    
    return obj_nil();
}

/* Method: die */
static Object *player_die(Object *self, Object **args, int argc) {
    (void)self; (void)args; (void)argc;
    
    Object *msg = obj_string("☠️  Game Over! You have died.", 35);
    send0(msg, SEL_PRINT);
    
    return obj_nil();
}

/* Method: new (class method - creates instance) */
static Object *player_new(Object *self, Object **args, int argc) {
    (void)self; (void)args; (void)argc;
    
    Object *instance = obj_instance(CLASS_PLAYER, PLAYER_FIELD_COUNT);
    
    /* Default values */
    obj_set_field(instance, PLAYER_FIELD_HEALTH, obj_int(100));
    obj_set_field(instance, PLAYER_FIELD_NAME, obj_string("Unnamed", 7));
    
    return instance;
}

/* Register the Player class */
static void register_player_class(void) {
    /* Intern selectors */
    SEL_TAKE_DAMAGE = selector_intern("take_damage:");
    SEL_DIE = selector_intern("die");
    
    /* Register class */
    CLASS_PLAYER = class_register("Player", PLAYER_FIELD_COUNT);
    
    /* Add methods */
    class_add_method(CLASS_PLAYER, SEL_NEW, player_new);
    class_add_method(CLASS_PLAYER, SEL_TAKE_DAMAGE, player_take_damage);
    class_add_method(CLASS_PLAYER, SEL_DIE, player_die);
}


/* =========================================================================
 * Main - What the compiler generates from 'method main do ... end'
 * ========================================================================= */

int main(void) {
    printf("=== Sprite Runtime Test ===\n\n");
    
    /* Initialize runtime with 1MB arena */
    runtime_init(1024 * 1024);
    
    /* Register game classes */
    register_player_class();
    
    printf("\n--- Creating Player ---\n");
    
    /* var hero = Player new */
    /* Note: In real impl, we'd send 'new' to the Player class object */
    Object *hero = player_new(NULL, NULL, 0);
    
    /* hero.name = "Roguemaster" */
    obj_set_field(hero, PLAYER_FIELD_NAME, obj_string("Roguemaster", 11));
    
    printf("Created: %s\n", obj_as_string(obj_get_field(hero, PLAYER_FIELD_NAME)));
    printf("Health: ");
    obj_print(obj_get_field(hero, PLAYER_FIELD_HEALTH));
    printf("\n");
    
    printf("\n--- Taking 30 Damage ---\n");
    send1(hero, SEL_TAKE_DAMAGE, obj_int(30));
    printf("Health after hit: ");
    obj_print(obj_get_field(hero, PLAYER_FIELD_HEALTH));
    printf("\n");
    
    printf("\n--- Taking 80 More Damage ---\n");
    send1(hero, SEL_TAKE_DAMAGE, obj_int(80));
    printf("Health after hit: ");
    obj_print(obj_get_field(hero, PLAYER_FIELD_HEALTH));
    printf("\n");
    
    printf("\n--- Memory Usage ---\n");
    printf("Used: %zu bytes\n", runtime_memory_used());
    printf("Remaining: %zu bytes\n", runtime_memory_remaining());
    
    printf("\n--- Testing Arena Reset (Simulating Level Transition) ---\n");
    runtime_reset();
    printf("Arena reset! All objects cleared.\n");
    printf("Used after reset: %zu bytes\n", runtime_memory_used());
    
    printf("\n--- Testing Tagged Pointers (Integer Performance) ---\n");
    printf("Creating 10,000 integers...\n");
    size_t before = runtime_memory_used();
    for (int i = 0; i < 10000; i++) {
        Object *n = obj_int(i);
        (void)n;  /* Compiler thinks we're using it */
    }
    size_t after = runtime_memory_used();
    printf("Memory used for 10,000 integers: %zu bytes\n", after - before);
    printf("(Should be 0 - they're all tagged pointers!)\n");
    
    /* Shutdown */
    runtime_shutdown();
    
    return 0;
}

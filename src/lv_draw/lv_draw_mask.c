/**
 * @file lv_mask.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_mask.h"
#include "../lv_misc/lv_math.h"
#include "../lv_misc/lv_log.h"
#include "../lv_core/lv_debug.h"

/*********************
 *      DEFINES
 *********************/
#define LV_MASK_MAX_NUM     16

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
    void * param;
    void * custom_id;
}lv_mask_saved_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_draw_mask_res_t lv_draw_mask_line(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_line_param_t * param);
static lv_draw_mask_res_t lv_draw_mask_radius(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_radius_param_t * param);
static lv_draw_mask_res_t lv_draw_mask_angle(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_angle_param_t * param);
static lv_draw_mask_res_t lv_draw_mask_fade(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_fade_param_t * param);
static lv_draw_mask_res_t lv_draw_mask_map(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_map_param_t * param);

static lv_draw_mask_res_t line_mask_flat(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_line_param_t * p);
static lv_draw_mask_res_t line_mask_steep(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_line_param_t * p);

static inline lv_opa_t mask_mix(lv_opa_t mask_act, lv_opa_t mask_new);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_mask_saved_t mask_list[LV_MASK_MAX_NUM];

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Add a draw mask. Everything drawn after it (until removing the mask) will be affected by the mask.
 * @param param an initialized mask parameter. Only the pointer is saved.
 * @param custom_id a custom pointer to identify the mask. Used in `lv_draw_mask_remove_custom`.
 * @return the an integer, the ID of the mask. Can be used in `lv_draw_mask_remove_id`.
 */
int16_t lv_draw_mask_add(void * param, void * custom_id)
{
    /*Look for a free entry*/
    uint8_t i;
    for(i = 0; i < LV_MASK_MAX_NUM; i++) {
        if(mask_list[i].param == NULL) break;
    }

    if(i >= LV_MASK_MAX_NUM) {
        LV_LOG_WARN("lv_mask_add: no place to add the mask");
        return LV_MASK_ID_INV;
    }

    mask_list[i].param = param;
    mask_list[i].custom_id = custom_id;

    return i;
}

/**
 * Apply the added buffers on a line. Used internally by the library's drawing routins.
 * @param mask_buf store the result mask here. Has to be `len` byte long. Should be initialized with `0xFF`.
 * @param abs_x absolute X coordinate where the line to calculate start
 * @param abs_y absolute Y coordinate where the line to calculate start
 * @param len length of the line to calculate (in pixel count)
 * @return Oneof these values:
 * - `LV_DRAW_MASK_RES_FULL_TRANSP`: the whole line is transparent. `mask_buf` is not set to zero
 * - `LV_DRAW_MASK_RES_FULL_COVER`: the whole line is fully visible. `mask_buf` is unchanged
 * - `LV_DRAW_MASK_RES_CHANGED`: `mask_buf` has changed, it shows the desired opacity of each pixel in the given line
 */
lv_draw_mask_res_t lv_draw_mask_apply(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len)
{
    bool changed = false;
    lv_draw_mask_common_dsc_t * dsc;

    lv_draw_mask_res_t res = LV_DRAW_MASK_RES_FULL_COVER;
    uint8_t i;
    for(i = 0; i < LV_MASK_MAX_NUM; i++) {
        if(mask_list[i].param) {
            dsc = mask_list[i].param;
            res = dsc->cb(mask_buf, abs_x, abs_y, len, (void*)mask_list[i].param);
            if(res == LV_DRAW_MASK_RES_FULL_TRANSP) return LV_DRAW_MASK_RES_FULL_TRANSP;
            else if(res == LV_DRAW_MASK_RES_CHANGED) changed = true;
        }
    }

    return changed ? LV_DRAW_MASK_RES_CHANGED : LV_DRAW_MASK_RES_FULL_COVER;
}

/**
 * Remove a mask with a given ID
 * @param id the ID of the mask.  Returned by `lv_draw_mask_add`
 * @return the parameter of the removed mask.
 * If more masks have `custom_id` ID then the last mask's parameter will be returned
 */
void * lv_draw_mask_remove_id(int16_t id)
{
    void * p = NULL;

    if(id != LV_MASK_ID_INV) {
        p = mask_list[id].param;
        mask_list[id].param = NULL;
        mask_list[id].custom_id = NULL;
    }

    return p;
}

/**
 * Remove all mask with a given custom ID
 * @param custom_id a pointer used in `lv_draw_mask_add`
 * @return return the parameter of the removed mask.
 * If more masks have `custom_id` ID then the last mask's parameter will be returned
 */
void * lv_draw_mask_remove_custom(void * custom_id)
{
    void * p = NULL;
    uint8_t i;
    for(i = 0; i < LV_MASK_MAX_NUM; i++) {
        if(mask_list[i].custom_id == custom_id) {
            p = mask_list[i].param;
            mask_list[i].param = NULL;
            mask_list[i].custom_id = NULL;
        }
    }
    return p;
}

/**
 * Count the currently added masks
 * @return number of active masks
 */
uint8_t lv_draw_mask_get_cnt(void)
{
    uint8_t cnt = 0;
    uint8_t i;
    for(i = 0; i < LV_MASK_MAX_NUM; i++) {
        if(mask_list[i].param) cnt++;
    }
    return cnt;
}

/**
 *Initialize a line mask from two points.
 * @param param pointer to a `lv_draw_mask_param_t` to initialize
 * @param p1x X coordinate of the first point of the line
 * @param p1y Y coordinate of the first point of the line
 * @param p2x X coordinate of the second point of the line
 * @param p2y y coordinate of the second point of the line
 * @param side and element of `lv_draw_mask_line_side_t` to describe which side to keep.
 * With `LV_DRAW_MASK_LINE_SIDE_LEFT/RIGHT` and horizontal line all pixels are kept
 * With `LV_DRAW_MASK_LINE_SIDE_TOP/BOTTOM` and vertical line all pixels are kept
 */
void lv_draw_mask_line_points_init(lv_draw_mask_line_param_t * param, lv_coord_t p1x, lv_coord_t p1y, lv_coord_t p2x, lv_coord_t p2y, lv_draw_mask_line_side_t side)
{
    memset(param, 0x00, sizeof(lv_draw_mask_line_param_t));

    if(p1y > p2y) {
        lv_coord_t t;
        t = p2x;
        p2x = p1x;
        p1x = t;

        t = p2y;
        p2y = p1y;
        p1y = t;
    }

    param->cfg.p1.x = p1x;
    param->cfg.p1.y = p1y;
    param->cfg.p2.x = p1x;
    param->cfg.p2.y = p1y;
    param->cfg.side = side;

    param->origo.x = p1x;
    param->origo.y = p1y;
    param->flat = (LV_MATH_ABS(p2x-p1x) > LV_MATH_ABS(p2y-p1y)) ? 1 : 0;
    param->yx_steep = 0;
    param->xy_steep = 0;
    param->dsc.cb = (lv_draw_mask_cb_t)lv_draw_mask_line;
    param->dsc.type = LV_DRAW_MASK_TYPE_LINE;

    lv_coord_t dx = p2x-p1x;
    lv_coord_t dy = p2y-p1y;

    if(param->flat) {
        /*Normalize the steep. Delta x should be relative to delta x = 1024*/
        int32_t m;

        if(dx) {
            m = (1 << 20) / dx;  /*m is multiplier to normalize y (upscaled by 1024)*/
            param->yx_steep = (m * dy) >> 10;
        }

        if(dy) {
            m = (1 << 20) / dy;  /*m is multiplier to normalize x (upscaled by 1024)*/
            param->xy_steep = (m * dx) >> 10;
        }
        param->steep = param->yx_steep;
    } else {
        /*Normalize the steep. Delta y should be relative to delta x = 1024*/
        int32_t m;

        if(dy) {
            m = (1 << 20) / dy;  /*m is multiplier to normalize x (upscaled by 1024)*/
            param->xy_steep = (m * dx) >> 10;
        }

        if(dx) {
            m = (1 << 20) / dx;  /*m is multiplier to normalize x (upscaled by 1024)*/
            param->yx_steep = (m * dy) >> 10;
        }
        param->steep = param->xy_steep;
    }

    if(param->cfg.side == LV_DRAW_MASK_LINE_SIDE_LEFT) param->inv = 0;
    else if(param->cfg.side == LV_DRAW_MASK_LINE_SIDE_RIGHT) param->inv = 1;
    else if(param->cfg.side == LV_DRAW_MASK_LINE_SIDE_TOP) {
        if(param->steep > 0) param->inv = 1;
        else param->inv = 0;
    }
    else if(param->cfg.side == LV_DRAW_MASK_LINE_SIDE_BOTTOM) {
        if(param->steep > 0) param->inv = 0;
        else param->inv = 1;
    }

    param->spx = param->steep >> 2;
    if(param->steep < 0) param->spx = -param->spx;
}

/**
 *Initialize a line mask from a point and an angle.
 * @param param pointer to a `lv_draw_mask_param_t` to initialize
 * @param px X coordiante of a point of the line
 * @param py X coordiante of a point of the line
 * @param angle right 0 deg, bottom: 90
 * @param side and element of `lv_draw_mask_line_side_t` to describe which side to keep.
 * With `LV_DRAW_MASK_LINE_SIDE_LEFT/RIGHT` and horizontal line all pixels are kept
 * With `LV_DRAW_MASK_LINE_SIDE_TOP/BOTTOM` and vertical line all pixels are kept
 */
void lv_draw_mask_line_angle_init(lv_draw_mask_line_param_t* param, lv_coord_t p1x, lv_coord_t py, int16_t angle, lv_draw_mask_line_side_t side)
{
    /* Find an optimal degree.
     * lv_mask_line_points_init will swap the points to keep the smaller y in p1
     * Theoretically a line with `angle` or `angle+180` is the same only the points are swapped
     * Find the degree which keeps the origo in place */
    if(angle > 180) angle -= 180; /*> 180 will swap the origo*/


    lv_coord_t p2x;
    lv_coord_t p2y;

    p2x = (lv_trigo_sin(angle + 90) >> 5) + p1x;
    p2y = (lv_trigo_sin(angle) >> 5) + py;

    lv_draw_mask_line_points_init(param, p1x, py, p2x, p2y, side);
}


/**
 * Initialize an angle mask.
 * @param param pointer to a `lv_draw_mask_param_t` to initialize
 * @param vertex_x X coordinate of the angle vertex (absolute coordinates)
 * @param vertex_y Y coordinate of the angle vertex (absolute coordinates)
 * @param start_angle start angle in degrees. 0 deg on the right, 90 deg, on the bottom
 * @param end_angle end angle
 */
void lv_draw_mask_angle_init(lv_draw_mask_angle_param_t * param, lv_coord_t vertex_x, lv_coord_t vertex_y, lv_coord_t start_angle, lv_coord_t end_angle)
{
    lv_draw_mask_line_side_t start_side;
    lv_draw_mask_line_side_t end_side;

    /* Constrain the input angles */
    if(start_angle < 0)
        start_angle = 0;
    else if(start_angle > 359)
        start_angle = 359;
    
    if(end_angle < 0)
        end_angle = 0;
    else if(end_angle > 359)
        end_angle = 359;

    if(end_angle < start_angle) {
        param->delta_deg = 360 - start_angle + end_angle;
    } else {
        param->delta_deg = LV_MATH_ABS(end_angle - start_angle);
    }

    param->cfg.start_angle = start_angle;
    param->cfg.end_angle = end_angle;
    param->cfg.vertex_p.x = vertex_x;
    param->cfg.vertex_p.y = vertex_y;
    param->dsc.cb = (lv_draw_mask_cb_t)lv_draw_mask_angle;
    param->dsc.type = LV_DRAW_MASK_TYPE_ANGLE;

    if(start_angle >= 0 && start_angle < 180) {
        start_side = LV_DRAW_MASK_LINE_SIDE_LEFT;
    }
    else if(start_angle >= 180 && start_angle < 360) {
        start_side = LV_DRAW_MASK_LINE_SIDE_RIGHT;
    } else {
        LV_DEBUG_ASSERT(false, "Unexpected start_angle", start_angle);
    }

    if(end_angle >= 0 && end_angle < 180) {
        end_side = LV_DRAW_MASK_LINE_SIDE_RIGHT;
    }
    else if(end_angle >= 180 && end_angle < 360) {
        end_side = LV_DRAW_MASK_LINE_SIDE_LEFT;
    } else {
        LV_DEBUG_ASSERT(false, "Unexpected end_angle", end_angle);
    }

    lv_draw_mask_line_angle_init(&param->start_line, vertex_x, vertex_y, start_angle, start_side);
    lv_draw_mask_line_angle_init(&param->end_line, vertex_x, vertex_y, end_angle, end_side);
}


/**
 * Initialize a fade mask.
 * @param param param pointer to a `lv_draw_mask_param_t` to initialize
 * @param rect coordinates of the rectangle to affect (absolute coordinates)
 * @param radius radius of the rectangle
 * @param inv: true: keep the pixels inside teh rectangle; keep teh pixels outside of the rectangle
 */
void lv_draw_mask_radius_init(lv_draw_mask_radius_param_t * param, const lv_area_t * rect, lv_coord_t radius, bool inv)
{

    lv_coord_t short_side = LV_MATH_MIN(lv_area_get_width(rect), lv_area_get_height(rect));
    if(radius > short_side >> 1) radius = short_side >> 1;

    lv_area_copy(&param->cfg.rect, rect);
    param->cfg.radius = radius;
    param->cfg.outer = inv ? 1 : 0;
    param->dsc.cb = (lv_draw_mask_cb_t)lv_draw_mask_radius;
    param->dsc.type = LV_DRAW_MASK_TYPE_RADIUS;
}


/**
 * Initialize a fade mask.
 * @param param pointer to a `lv_draw_mask_param_t` to initialize
 * @param coords coordinates of the area to affect (absolute coordinates)
 * @param opa_top opacity on the top
 * @param y_top at which coordinate start to change to opacity to `opa_bottom`
 * @param opa_bottom opacity at the bottom
 * @param y_bottom at which coordinate reach `opa_bottom`.
 */
void lv_draw_mask_fade_init(lv_draw_mask_fade_param_t * param, lv_area_t * coords, lv_opa_t opa_top, lv_coord_t y_top, lv_opa_t opa_bottom, lv_coord_t y_bottom)
{
    lv_area_copy(&param->cfg.coords, coords);
    param->cfg.opa_top= opa_top;
    param->cfg.opa_bottom = opa_bottom;
    param->cfg.y_top= y_top;
    param->cfg.y_bottom = y_bottom;
    param->dsc.cb = (lv_draw_mask_cb_t)lv_draw_mask_fade;
    param->dsc.type = LV_DRAW_MASK_TYPE_FADE;
}


/**
 * Initialize a map mask.
 * @param param pointer to a `lv_draw_mask_param_t` to initialize
 * @param coords coordinates of the map (absolute coordinates)
 * @param map array of bytes with the mask values
 */
void lv_draw_mask_map_init(lv_draw_mask_map_param_t * param, lv_area_t * coords, const lv_opa_t * map)
{
    lv_area_copy(&param->cfg.coords, coords);
    param->cfg.map = map;
    param->dsc.cb = (lv_draw_mask_cb_t)lv_draw_mask_map;
    param->dsc.type = LV_DRAW_MASK_TYPE_MAP;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

static lv_draw_mask_res_t lv_draw_mask_line(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_line_param_t * p)
{
    /*Make to points relative to the vertex*/
    abs_y -= p->origo.y;
    abs_x -= p->origo.x;

    /*Handle special cases*/
    if(p->steep == 0) {
        /*Horizontal*/
        if(p->flat) {
            /*Non sense: Can't be on the right/left of a horizontal line*/
            if(p->cfg.side == LV_DRAW_MASK_LINE_SIDE_LEFT || p->cfg.side == LV_DRAW_MASK_LINE_SIDE_RIGHT) return LV_DRAW_MASK_RES_FULL_COVER;
            else if(p->cfg.side == LV_DRAW_MASK_LINE_SIDE_TOP && abs_y+1 < 0) return LV_DRAW_MASK_RES_FULL_COVER;
            else if(p->cfg.side == LV_DRAW_MASK_LINE_SIDE_BOTTOM && abs_y > 0) return LV_DRAW_MASK_RES_FULL_COVER;
            else {
                return LV_DRAW_MASK_RES_FULL_TRANSP;
            }
        }
        /*Vertical*/
        else {
            /*Non sense: Can't be on the top/bottom of a vertical line*/
            if(p->cfg.side == LV_DRAW_MASK_LINE_SIDE_TOP || p->cfg.side == LV_DRAW_MASK_LINE_SIDE_BOTTOM) return LV_DRAW_MASK_RES_FULL_COVER;
            else if(p->cfg.side == LV_DRAW_MASK_LINE_SIDE_RIGHT && abs_x > 0) return LV_DRAW_MASK_RES_FULL_COVER;
            else if(p->cfg.side == LV_DRAW_MASK_LINE_SIDE_LEFT) {
                if(abs_x + len < 0) return LV_DRAW_MASK_RES_FULL_COVER;
                else {
                    int32_t k = - abs_x;
                    if(k < 0) return LV_DRAW_MASK_RES_FULL_TRANSP;
                    if(k >= 0 && k < len) memset(&mask_buf[k], 0x00, len - k);
                    return  LV_DRAW_MASK_RES_CHANGED;
                }
            }
            else {
                if(abs_x + len < 0) return LV_DRAW_MASK_RES_FULL_TRANSP;
                else {
                    int32_t k = - abs_x;
                    if(k < 0) k = 0;
                    if(k >= len) return LV_DRAW_MASK_RES_FULL_TRANSP;
                    else if(k >= 0 && k < len) memset(&mask_buf[0], 0x00,k);
                    return  LV_DRAW_MASK_RES_CHANGED;
                }
            }
        }
    }

    lv_draw_mask_res_t res;
    if(p->flat) {
        res = line_mask_flat(mask_buf, abs_x, abs_y, len, p);
    } else {
        res = line_mask_steep(mask_buf, abs_x, abs_y, len, p);
    }

    return res;
}

static lv_draw_mask_res_t line_mask_flat(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_line_param_t * p)
{
    lv_coord_t y_at_x;
    y_at_x = (int32_t)((int32_t)p->yx_steep * abs_x) >> 10;

    if(p->yx_steep > 0) {
        if(y_at_x > abs_y) {
            if(p->inv) {
                return LV_DRAW_MASK_RES_FULL_COVER;
            } else {
                return LV_DRAW_MASK_RES_FULL_TRANSP;
            }
        }
    } else {
        if(y_at_x < abs_y) {
            if(p->inv) {
                return LV_DRAW_MASK_RES_FULL_COVER;
            } else {
                return LV_DRAW_MASK_RES_FULL_TRANSP;
            }
        }
    }

    /* At the end of the mask if the limit line is smaller then the mask's y.
     * Then the mask is in the "good" area*/
    y_at_x = (int32_t)((int32_t)p->yx_steep * (abs_x + len)) >> 10;
    if(p->yx_steep > 0) {
        if(y_at_x < abs_y) {
            if(p->inv) {
                return LV_DRAW_MASK_RES_FULL_TRANSP;
            } else {
                return LV_DRAW_MASK_RES_FULL_COVER;
            }
        }
    } else {
        if(y_at_x > abs_y) {
            if(p->inv) {
                return LV_DRAW_MASK_RES_FULL_TRANSP;
            } else {
                return LV_DRAW_MASK_RES_FULL_COVER;
            }
        }
    }

    int32_t xe;
    if(p->yx_steep > 0) xe = ((abs_y << 8) * p->xy_steep) >> 10;
    else xe = (((abs_y + 1) << 8) * p->xy_steep) >> 10;

    int32_t xei = xe >> 8;
    int32_t xef = xe & 0xFF;

    int32_t px_h;
    if(xef == 0) px_h = 255;
    else px_h = 255 - (((255 - xef) * p->spx) >> 8);
    int32_t k = xei - abs_x;
    lv_opa_t m;

    if(xef) {
        if(k >= 0 && k < len) {
            m = 255 - (((255-xef) * (255 - px_h)) >> 9);
            if(p->inv) m = 255 - m;
            mask_buf[k] = mask_mix(mask_buf[k], m);
        }
        k++;
    }

    while(px_h > p->spx) {
        if(k >= 0 && k < len) {
            m = px_h - (p->spx >> 1);
            if(p->inv) m = 255 - m;
            mask_buf[k] = mask_mix(mask_buf[k], m);
        }
        px_h -= p->spx;
        k++;
        if(k >= len) break;
    }


    if(k < len && k >= 0) {
        int32_t x_inters = (px_h * p->xy_steep) >> 10;
        m = (x_inters * px_h) >> 9;
        if(p->yx_steep < 0) m = 255 - m;
        if(p->inv) m = 255 - m;
        mask_buf[k] = mask_mix(mask_buf[k], m);
    }


    if(p->inv) {
        k = xei - abs_x;
        if(k > len) {
            return LV_DRAW_MASK_RES_FULL_TRANSP;
        }
        if(k >= 0)
        {
            memset(&mask_buf[0], 0x00,  k);
        }
    } else {
        k++;
        if(k < 0) {
            return LV_DRAW_MASK_RES_FULL_TRANSP;
        }
        if(k <= len) {
            memset(&mask_buf[k], 0x00,  len - k);
        }
    }

    return LV_DRAW_MASK_RES_CHANGED;
}

static lv_draw_mask_res_t line_mask_steep(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_line_param_t * p)
{
    int32_t k;
    lv_coord_t x_at_y;
    /* At the beginning of the mask if the limit line is greater then the mask's y.
     * Then the mask is in the "wrong" area*/
    x_at_y = (int32_t)((int32_t)p->xy_steep * abs_y) >> 10;
    if(p->xy_steep > 0) x_at_y++;
    if(x_at_y < abs_x) {
        if(p->inv) {
            return LV_DRAW_MASK_RES_FULL_COVER;
        } else {
            return LV_DRAW_MASK_RES_FULL_TRANSP;
        }
    }

    /* At the end of the mask if the limit line is smaller then the mask's y.
     * Then the mask is in the "good" area*/
    x_at_y = (int32_t)((int32_t)p->xy_steep * (abs_y)) >> 10;
    if(x_at_y > abs_x + len) {
        if(p->inv) {
            return LV_DRAW_MASK_RES_FULL_TRANSP;
        } else {
            return LV_DRAW_MASK_RES_FULL_COVER;
        }
    }

    int32_t xe = ((abs_y << 8) * p->xy_steep) >> 10;
    int32_t xei = xe >> 8;
    int32_t xef = xe & 0xFF;

    int32_t xq = (((abs_y + 1) << 8) * p->xy_steep) >> 10;
    int32_t xqi = xq >> 8;
    int32_t xqf = xq & 0xFF;

    lv_opa_t m;

    k = xei - abs_x;
    if(xei != xqi && (p->xy_steep < 0 && xef == 0)) {
        xef = 0xFF;
        xei = xqi;
        k--;
    }

    if(xei == xqi) {
        if(k >= 0 && k < len) {
            m = (xef + xqf) >> 1;
            if(p->inv) m = 255 - m;
            mask_buf[k] = mask_mix(mask_buf[k], m);
        }
        k++;

        if(p->inv) {
            k = xei - abs_x;
            if(k >= len) {
                return LV_DRAW_MASK_RES_FULL_TRANSP;
            }
            if(k >= 0) memset(&mask_buf[0], 0x00,  k);

        } else {
            if(k > len) k = len;
            if(k == 0) return LV_DRAW_MASK_RES_FULL_TRANSP;
            else if(k > 0) memset(&mask_buf[k] ,0x00,  len - k);
        }

    } else {
        int32_t y_inters;
        if(p->xy_steep < 0) {
            y_inters = (xef * (-p->yx_steep)) >> 10;
            if(k >= 0 && k < len ) {
                m = (y_inters * xef) >> 9;
                if(p->inv) m = 255 - m;
                mask_buf[k] = mask_mix(mask_buf[k], m);
            }
            k--;

            int32_t x_inters = ((255-y_inters) * (-p->xy_steep)) >> 10;

            if(k >= 0 && k < len ) {
                m = 255-(((255-y_inters) * x_inters) >> 9);
                if(p->inv) m = 255 - m;
                mask_buf[k] = mask_mix(mask_buf[k], m);
            }

            k+=2;

            if(p->inv) {
                k = xei - abs_x - 1;

                if(k > len) k = len;
                else if(k > 0) memset(&mask_buf[0], 0x00,  k);

            } else {
                if(k > len) return LV_DRAW_MASK_RES_FULL_COVER;
                if(k >= 0) memset(&mask_buf[k] ,0x00,  len - k);
            }

        } else {
            y_inters = ((255-xef) * p->yx_steep) >> 10;
            if(k >= 0 && k < len ) {
                m = 255 - ((y_inters * (255-xef)) >> 9);
                if(p->inv) m = 255 - m;
                mask_buf[k] = mask_mix(mask_buf[k], m);
            }

            k++;

            int32_t x_inters = ((255-y_inters) * p->xy_steep) >> 10;
            if(k >= 0 && k < len ) {
                m = ((255-y_inters) * x_inters) >> 9;
                if(p->inv) m = 255 - m;
                mask_buf[k] = mask_mix(mask_buf[k], m);
            }
            k++;

            if(p->inv) {
                k = xei - abs_x;
                if(k > len)  return LV_DRAW_MASK_RES_FULL_TRANSP;
                if(k >= 0) memset(&mask_buf[0], 0x00,  k);

            } else {
                if(k > len) k = len;
                if(k == 0) return LV_DRAW_MASK_RES_FULL_TRANSP;
                else if(k > 0) memset(&mask_buf[k] ,0x00,  len - k);
            }

        }
    }

    return LV_DRAW_MASK_RES_CHANGED;
}


static lv_draw_mask_res_t lv_draw_mask_angle(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_angle_param_t * p)
{
    lv_coord_t rel_y = abs_y - p->cfg.vertex_p.y;
    lv_coord_t rel_x = abs_x - p->cfg.vertex_p.x;


    if(p->cfg.start_angle < 180 && p->cfg.end_angle < 180 &&
       p->cfg.start_angle != 0  && p->cfg.end_angle != 0 &&
       p->cfg.start_angle > p->cfg.end_angle) {

        if(abs_y < p->cfg.vertex_p.y) {
            return LV_DRAW_MASK_RES_FULL_COVER;
        }

        /*Start angle mask can work only from the end of end angle mask */
        lv_coord_t end_angle_first = (rel_y * p->end_line.xy_steep) >> 10;
        lv_coord_t start_angle_last= ((rel_y+1) * p->start_line.xy_steep) >> 10;


        /*Do not let the line end cross the vertex else it will affect the opposite part*/
        if(p->cfg.start_angle > 270 && p->cfg.start_angle <= 359 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.start_angle > 0 && p->cfg.start_angle <= 90 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.start_angle > 90 && p->cfg.start_angle < 270 && start_angle_last > 0) start_angle_last = 0;

        if(p->cfg.end_angle > 270 && p->cfg.end_angle <= 359 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.end_angle > 0 &&   p->cfg.end_angle <= 90 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.end_angle > 90 &&  p->cfg.end_angle < 270 && start_angle_last > 0) start_angle_last = 0;


        int32_t dist = (end_angle_first - start_angle_last) >> 1;

        lv_draw_mask_res_t res1 = LV_DRAW_MASK_RES_FULL_COVER;
        lv_draw_mask_res_t res2 = LV_DRAW_MASK_RES_FULL_COVER;

        int32_t tmp = start_angle_last + dist - rel_x;
        if(tmp > len) tmp = len;
        if(tmp > 0) {
            res1 = lv_draw_mask_line(&mask_buf[0], abs_x, abs_y, tmp, &p->start_line);
            if(res1 == LV_DRAW_MASK_RES_FULL_TRANSP) {
                memset(&mask_buf[0], 0x00, tmp);
            }
        }

        if(tmp > len) tmp = len;
        if(tmp < 0) tmp = 0;
        res2 = lv_draw_mask_line(&mask_buf[tmp], abs_x+tmp, abs_y, len-tmp, &p->end_line);
        if(res2 == LV_DRAW_MASK_RES_FULL_TRANSP) {
            memset(&mask_buf[tmp], 0x00, len-tmp);
        }
        if(res1 == res2) return res1;
        else return LV_DRAW_MASK_RES_CHANGED;
    }
    else if(p->cfg.start_angle > 180 && p->cfg.end_angle > 180 && p->cfg.start_angle > p->cfg.end_angle) {

        if(abs_y > p->cfg.vertex_p.y) {
            return LV_DRAW_MASK_RES_FULL_COVER;
        }

        /*Start angle mask can work only from the end of end angle mask */
        lv_coord_t end_angle_first = (rel_y * p->end_line.xy_steep) >> 10;
        lv_coord_t start_angle_last= ((rel_y+1) * p->start_line.xy_steep) >> 10;

        /*Do not let the line end cross the vertex else it will affect the opposite part*/
        if(p->cfg.start_angle > 270 && p->cfg.start_angle <= 359 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.start_angle > 0 && p->cfg.start_angle <= 90 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.start_angle > 90 && p->cfg.start_angle < 270 && start_angle_last > 0) start_angle_last = 0;

        if(p->cfg.end_angle > 270 && p->cfg.end_angle <= 359 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.end_angle > 0 &&   p->cfg.end_angle <= 90 && start_angle_last < 0) start_angle_last = 0;
        else if(p->cfg.end_angle > 90 &&  p->cfg.end_angle < 270 && start_angle_last > 0) start_angle_last = 0;

        int32_t dist = (end_angle_first - start_angle_last) >> 1;

        lv_draw_mask_res_t res1 = LV_DRAW_MASK_RES_FULL_COVER;
        lv_draw_mask_res_t res2 = LV_DRAW_MASK_RES_FULL_COVER;

        int32_t tmp = start_angle_last + dist - rel_x;
        if(tmp > len) tmp = len;
        if(tmp > 0) {
            res1 = lv_draw_mask_line(&mask_buf[0], abs_x, abs_y, tmp, (lv_draw_mask_line_param_t*)&p->end_line);
            if(res1 == LV_DRAW_MASK_RES_FULL_TRANSP) {
                memset(&mask_buf[0], 0x00, tmp);
            }
        }

        if(tmp > len) tmp = len;
        if(tmp < 0) tmp = 0;
        res2 = lv_draw_mask_line(&mask_buf[tmp], abs_x+tmp, abs_y, len-tmp, (lv_draw_mask_line_param_t*)&p->start_line);
        if(res2 == LV_DRAW_MASK_RES_FULL_TRANSP) {
            memset(&mask_buf[tmp], 0x00, len-tmp);
        }
        if(res1 == res2) return res1;
        else return LV_DRAW_MASK_RES_CHANGED;
    }
    else  {

        lv_draw_mask_res_t res1 = LV_DRAW_MASK_RES_FULL_COVER;
        lv_draw_mask_res_t res2 = LV_DRAW_MASK_RES_FULL_COVER;

        if(p->cfg.start_angle == 180) {
            if(abs_y < p->cfg.vertex_p.y) res1 = LV_DRAW_MASK_RES_FULL_COVER;
            else res1 = LV_DRAW_MASK_RES_UNKNOWN;
        }
        else if(p->cfg.start_angle == 0) {
            if(abs_y < p->cfg.vertex_p.y) res1 = LV_DRAW_MASK_RES_UNKNOWN;
            else res1 = LV_DRAW_MASK_RES_FULL_COVER;
        }
        else if((p->cfg.start_angle < 180 && abs_y < p->cfg.vertex_p.y) ||
                (p->cfg.start_angle > 180 && abs_y >= p->cfg.vertex_p.y)) {
            res1 = LV_DRAW_MASK_RES_UNKNOWN;
        }
        else  {
            res1 = lv_draw_mask_line(mask_buf, abs_x, abs_y, len, &p->start_line);
        }

        if(p->cfg.end_angle == 180) {
            if(abs_y < p->cfg.vertex_p.y) res2 = LV_DRAW_MASK_RES_UNKNOWN;
            else res2 = LV_DRAW_MASK_RES_FULL_COVER;
        }
        else if(p->cfg.end_angle == 0) {
            if(abs_y < p->cfg.vertex_p.y) res2 = LV_DRAW_MASK_RES_FULL_COVER;
            else res2 = LV_DRAW_MASK_RES_UNKNOWN;
        }
        else if((p->cfg.end_angle < 180 && abs_y < p->cfg.vertex_p.y) ||
                (p->cfg.end_angle > 180 && abs_y >= p->cfg.vertex_p.y)) {
            res2 = LV_DRAW_MASK_RES_UNKNOWN;
        }
        else {
            res2 = lv_draw_mask_line(mask_buf, abs_x, abs_y, len, &p->end_line);
        }

        if(res1 == LV_DRAW_MASK_RES_FULL_TRANSP || res2 == LV_DRAW_MASK_RES_FULL_TRANSP) return LV_DRAW_MASK_RES_FULL_TRANSP;
        else if(res1 == LV_DRAW_MASK_RES_UNKNOWN && res2 == LV_DRAW_MASK_RES_UNKNOWN) return LV_DRAW_MASK_RES_FULL_TRANSP;
        else if(res1 == LV_DRAW_MASK_RES_FULL_COVER &&  res2 == LV_DRAW_MASK_RES_FULL_COVER) return LV_DRAW_MASK_RES_FULL_COVER;
        else return LV_DRAW_MASK_RES_CHANGED;
    }
}

static lv_draw_mask_res_t lv_draw_mask_radius(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_radius_param_t * p)
{
    if(p->cfg.outer == 0) {
        if(abs_y < p->cfg.rect.y1 || abs_y > p->cfg.rect.y2) {
            return LV_DRAW_MASK_RES_FULL_TRANSP;
        }
    } else {
        if(abs_y < p->cfg.rect.y1 || abs_y > p->cfg.rect.y2) {
            return LV_DRAW_MASK_RES_FULL_COVER;
        }
    }

    if((abs_x >= p->cfg.rect.x1 + p->cfg.radius && abs_x + len <= p->cfg.rect.x2 - p->cfg.radius) ||
            (abs_y >= p->cfg.rect.y1 + p->cfg.radius && abs_y <= p->cfg.rect.y2 - p->cfg.radius)) {
        if(p->cfg.outer == 0) {
            /*Remove the edges*/
            int32_t last =  p->cfg.rect.x1 - abs_x;
            if(last > len) return LV_DRAW_MASK_RES_FULL_TRANSP;
            if(last >= 0) {
                memset(&mask_buf[0], 0x00, last);
            }

            int32_t first = p->cfg.rect.x2 - abs_x + 1;
            if(first <= 0) return LV_DRAW_MASK_RES_FULL_TRANSP;
            else if(first < len) {
                memset(&mask_buf[first], 0x00, len-first);
            }
            if(last == 0 && first == len) return LV_DRAW_MASK_RES_FULL_COVER;
            else return LV_DRAW_MASK_RES_CHANGED;
        }
        else {
            int32_t first = p->cfg.rect.x1 - abs_x;
            if(first < 0) first = 0;
            if(first <= len) {
                int32_t last =  p->cfg.rect.x2 - abs_x - first + 1;
                if(first + last > len) last = len - first;
                if(last >= 0) {
                    memset(&mask_buf[first], 0x00, last);
                }
            }
        }
        return LV_DRAW_MASK_RES_CHANGED;
    }

    int32_t k = p->cfg.rect.x1 -abs_x; /*First relevant coordinate on the of the mask*/
    lv_coord_t w = lv_area_get_width(&p->cfg.rect);
    lv_coord_t h = lv_area_get_height(&p->cfg.rect);
    abs_x -= p->cfg.rect.x1;
    abs_y -= p->cfg.rect.y1;

    uint32_t r2 = p->cfg.radius * p->cfg.radius;

    /*Handle corner areas*/
    if(abs_y < p->cfg.radius || abs_y > h - p->cfg.radius - 1) {
        /* y = 0 should mean the top of the circle */
        lv_coord_t y;
        if(abs_y < p->cfg.radius)  y = p->cfg.radius - abs_y;
        else y = p->cfg.radius - (h - abs_y) + 1;

        /* Get the x intersection points for `abs_y` and `abs_y+1`
         * Use the circle's equation x = sqrt(r^2 - y^2) */
        lv_sqrt_res_t x0;
        lv_sqrt(r2 - (y * y), &x0);

        lv_sqrt_res_t x1;
        lv_sqrt(r2 - ((y-1) * (y-1)), &x1);

        /* If x1 is on the next round coordinate (e.g. x0: 3.5, x1:4.0)
         * then treat x1 as x1: 3.99 to handle them as they were on the same pixel*/
        if(x0.i == x1.i - 1 && x1.f == 0) {
            x1.i--;
            x1.f = 0xFF;
        }

        /*If the two x intersections are on the same x then just get average of the fractionals*/
        if(x0.i == x1.i) {
            lv_opa_t m = (x0.f + x1.f) >> 1;
            if(p->cfg.outer) m = 255 - m;
            int32_t ofs = p->cfg.radius - x0.i - 1;

            /*Left corner*/
            int32_t kl = k + ofs;

            if(kl >= 0 && kl < len) {
                mask_buf[kl] = mask_mix(mask_buf[kl], m);
            }

            /*Right corner*/
            int32_t kr = k+(w-ofs-1);
            if(kr >= 0 && kr < len) {
                mask_buf[kr] = mask_mix(mask_buf[kr], m);
            }

            /*Clear the unused parts*/
            if(p->cfg.outer == 0) {
                kr++;
                if(kl > len)  {
                    return LV_DRAW_MASK_RES_FULL_TRANSP;
                }
                if(kl >= 0) {
                    memset(&mask_buf[0], 0x00, kl);
                }
                if(kr < 0) {
                    return LV_DRAW_MASK_RES_FULL_TRANSP;
                }
                if(kr <= len) {
                    memset(&mask_buf[kr], 0x00, len-kr);
                }
            } else {
                kl++;
                int32_t first = kl;
                if(first < 0) first = 0;

                int32_t len_tmp = kr-first;
                if(len_tmp + first > len) len_tmp = len - first;
                if(first < len && len_tmp >= 0) {
                    memset(&mask_buf[first], 0x00, len_tmp);
                }
            }
        }
        /*Multiple pixels are affected. Get y intersection of the pixels*/
        else {
            int32_t ofs = p->cfg.radius - (x0.i + 1);
            int32_t kl = k + ofs;
            int32_t kr = k + (w - ofs -1);

            if(p->cfg.outer) {
                int32_t first = kl + 1;
                if(first < 0) first = 0;

                int32_t len_tmp = kr-first;
                if(len_tmp + first > len) len_tmp = len - first;
                if(first < len && len_tmp >= 0) {
                    memset(&mask_buf[first], 0x00, len_tmp);
                }
            }

            uint32_t i = x0.i + 1;
            lv_opa_t m;
            lv_sqrt_res_t y_prev;
            lv_sqrt_res_t y_next;

            lv_sqrt(r2 - (x0.i * x0.i), &y_prev);

            if(y_prev.f == 0) {
                y_prev.i--;
                y_prev.f = 0xFF;
            }

            /*The first y intersection is special as it might be in the previous line*/
            if(y_prev.i >= y) {
                lv_sqrt(r2 - (i * i), &y_next);
                m = 255 - (((255-x0.f) * (255 - y_next.f)) >> 9);

                if(p->cfg.outer) m = 255 - m;
                if(kl >= 0 && kl < len) mask_buf[kl] = mask_mix(mask_buf[kl], m);
                if(kr >= 0 && kr < len) mask_buf[kr] = mask_mix(mask_buf[kr], m);
                kl--;
                kr++;
                y_prev.f = y_next.f;
                i++;
            }

            /*Set all points which are crossed by the circle*/
            for(; i <= x1.i; i++) {
                lv_sqrt(r2 - (i * i), &y_next);

                m = (y_prev.f + y_next.f) >> 1;
                if(p->cfg.outer) m = 255 - m;
                if(kl >= 0 && kl < len) mask_buf[kl] = mask_mix(mask_buf[kl], m);
                if(kr >= 0 && kr < len) mask_buf[kr] = mask_mix(mask_buf[kr], m);
                kl--;
                kr++;
                y_prev.f = y_next.f;
            }

            /*If the last pixel was left in its middle therefore
             * the circle still has parts on the next one*/
            if(y_prev.f) {
                m = (y_prev.f * x1.f) >> 9;
                if(p->cfg.outer) m = 255 - m;
                if(kl >= 0 && kl < len) mask_buf[kl] = mask_mix(mask_buf[kl], m);
                if(kr >= 0 && kr < len) mask_buf[kr] = mask_mix(mask_buf[kr], m);
                kl--;
                kr++;
            }

            if(p->cfg.outer == 0) {
                kl++;
                if(kl > len) {
                    return LV_DRAW_MASK_RES_FULL_TRANSP;
                }
                if(kl >= 0) memset(&mask_buf[0], 0x00, kl);

                if(kr < 0) {
                    return LV_DRAW_MASK_RES_FULL_TRANSP;
                }
                if(kr < len) memset(&mask_buf[kr], 0x00, len - kr);
            }
        }
    }

    return LV_DRAW_MASK_RES_CHANGED;
}


static lv_draw_mask_res_t lv_draw_mask_fade(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_fade_param_t * p)
{
    if(abs_y < p->cfg.coords.y1) return LV_DRAW_MASK_RES_FULL_COVER;
    if(abs_y > p->cfg.coords.y2) return LV_DRAW_MASK_RES_FULL_COVER;
    if(abs_x + len < p->cfg.coords.x1) return LV_DRAW_MASK_RES_FULL_COVER;
    if(abs_x > p->cfg.coords.x2) return LV_DRAW_MASK_RES_FULL_COVER;

    if(abs_x + len > p->cfg.coords.x2) len -= abs_x + len - p->cfg.coords.x2 - 1;

    if(abs_x < p->cfg.coords.x1) {
        lv_coord_t x_ofs = 0;
        x_ofs = p->cfg.coords.x1 - abs_x;
        len -= x_ofs;
        mask_buf += x_ofs;
    }

    lv_coord_t i;

    if(abs_y <= p->cfg.y_top) {
        for(i = 0; i < len; i++) {
            mask_buf[i] = mask_mix(mask_buf[i], p->cfg.opa_top);
        }
        return LV_DRAW_MASK_RES_CHANGED;
    } else if(abs_y >= p->cfg.y_bottom) {
        for(i = 0; i < len; i++) {
            mask_buf[i] = mask_mix(mask_buf[i], p->cfg.opa_bottom);
        }
        return LV_DRAW_MASK_RES_CHANGED;
    } else {
        /*Calculate the opa proportionally*/
        int16_t opa_diff = p->cfg.opa_bottom - p->cfg.opa_top;
        lv_coord_t y_diff = p->cfg.y_bottom - p->cfg.y_top + 1;
        lv_opa_t opa_act = (int32_t)((int32_t)(abs_y - p->cfg.y_top) * opa_diff) / y_diff;
        opa_act += p->cfg.opa_top;

        for(i = 0; i < len; i++) {
            mask_buf[i] = mask_mix(mask_buf[i], opa_act);
        }
        return LV_DRAW_MASK_RES_CHANGED;


    }
    return LV_DRAW_MASK_RES_FULL_COVER;
}

static lv_draw_mask_res_t lv_draw_mask_map(lv_opa_t * mask_buf, lv_coord_t abs_x, lv_coord_t abs_y, lv_coord_t len, lv_draw_mask_map_param_t * p)
{
    /*Handle out of the mask cases*/
    if(abs_y < p->cfg.coords.y1) return LV_DRAW_MASK_RES_FULL_COVER;
    if(abs_y > p->cfg.coords.y2) return LV_DRAW_MASK_RES_FULL_COVER;
    if(abs_x + len < p->cfg.coords.x1) return LV_DRAW_MASK_RES_FULL_COVER;
    if(abs_x > p->cfg.coords.x2) return LV_DRAW_MASK_RES_FULL_COVER;

    /*Got to the current row in the map*/
    const lv_opa_t * map_tmp = p->cfg.map;
    map_tmp += (abs_y - p->cfg.coords.y1) * lv_area_get_width(&p->cfg.coords);


    if(abs_x + len > p->cfg.coords.x2) len -= abs_x + len - p->cfg.coords.x2 - 1;

    if(abs_x < p->cfg.coords.x1) {
        lv_coord_t x_ofs = 0;
        x_ofs = p->cfg.coords.x1 - abs_x;
        len -= x_ofs;
        mask_buf += x_ofs;
    } else {
        map_tmp += (abs_x - p->cfg.coords.x1);
    }

    lv_coord_t i;
    for(i = 0; i < len; i++) {
        mask_buf[i] = mask_mix(mask_buf[i], map_tmp[i]);
    }

    return LV_DRAW_MASK_RES_CHANGED;
}


static inline lv_opa_t mask_mix(lv_opa_t mask_act, lv_opa_t mask_new)
{
    if(mask_new > LV_OPA_MAX) return mask_act;
    if(mask_new < LV_OPA_MIN) return 0;

    return (uint16_t)((uint16_t) (mask_act * mask_new) >> 8);

}

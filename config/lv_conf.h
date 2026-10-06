/* LVGL v9.5.0 host configuration for the AI Passport simulator.
 * Only deviations from LVGL defaults are listed; everything else follows
 * lv_conf_internal.h. Keep this file minimal and version-pinned. */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (64U * 1024U)
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_FONT_MONTSERRAT_14 1

/* Host tools must fail loudly: upstream halts (while(1)) on assert. */
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();

#endif /* LV_CONF_H */

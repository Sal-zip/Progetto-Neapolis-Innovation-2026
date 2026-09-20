/*
    NISC2026 - GestureLogicv3
    Modulo: riconoscimento gesture con VL53L7CX (motion indicator 4x4).

    PUBLISHER (pattern publisher/subscriber, contratto in gesture_events.h):
    il thread interno fa tutto da solo (inizializza il sensore, acquisisce i
    frame di motion e riconosce i gesti) e, a gesto completato, fa broadcast
    della flag corrispondente su `gesture_events`. Nessun consumatore.

    Macchina a stati WAKE (sistema SPENTO -> ATTIVO):
      doppio movimento della mano: due picchi di motion totale con una pausa
      tra loro, entro una finestra di timeout. L'avanzamento viene stampato
      su dbg per guidare chi esegue il gesto.

    Macchina a stati ACTION (sistema ATTIVO):
      azioni laterali "apri/chiudi": picco di motion dominante su una delle
      due zone laterali (40% - 20% - 40%) seguito dal ritorno della mano
      (zona sotto la soglia di pausa). Completa -> evento LEFT/RIGHT.

    Il sistema una volta attivato resta attivo (nessuna disattivazione,
    come nella versione validata su hardware).
*/

#include "gesture.h"
#include "gesture_events.h"
#include "chprintf.h"

#include <stdbool.h>

#include "vl53l7cx_api.h"
#include "vl53l7cx_plugin_motion_indicator.h"

/* ========================================================================= */
/* Configurazione hardware (MODIFICARE per adattarla al cablaggio reale)     */
/* ========================================================================= */

/* Bus I2C e indirizzo del VL53L7CX. */
#define GESTURE_I2C_BUS     &I2CD1
#define GESTURE_I2C_ADDRESS 0x29U

/* Linee SCL/SDA (I2C1 su PB8/PB9, alternate 4, open-drain). */
#define GESTURE_SCL_LINE    PAL_LINE(GPIOB, 8U)
#define GESTURE_SDA_LINE    PAL_LINE(GPIOB, 9U)

/* Intervallo di polling dello stato del sensore (ms). */
#define GESTURE_POLL_MS     20U

/* Frequenza di ranging: alzata a 15 Hz per fluidita' nel tracciamento. */
#define GESTURE_RANGING_HZ  15U

/* Griglia del motion indicator (fissa a 4x4). */
#define GESTURE_ZONES       16U
#define GESTURE_COLS        4U

/* ========================================================================= */
/* Soglie del riconoscimento (valori calibrati, NON cambiare se non per      */
/* prove mirate)                                                             */
/* ========================================================================= */

/* Finestra (in frame) per completare il doppio movimento di wake-up.        */
#define WAKE_TIMEOUT_FRAMES 60U

/* Energia di motion totale per considerare la mano "in movimento" (picco).  */
#define WAKE_PEAK_TOTAL     5000U

/* Sotto questa energia la mano e' "ferma" (pausa tra i due movimenti).      */
#define WAKE_PAUSE_TOTAL    400U

/* Picco richiesto nella singola zona (Dx o Sx) per iniziare un'azione.      */
#define ACTION_PEAK_ZONE    2500U

/* Movimento basso richiesto per confermare il ritorno della mano.           */
#define ACTION_PAUSE_ZONE   100U

/* ~2 secondi per completare un'azione laterale.                             */
#define ACTION_TIMEOUT_FRAMES 30U

/* Pausa (in frame) dopo un'azione o dopo l'attivazione.                     */
#define COOLDOWN_FRAMES     25U

/* ========================================================================= */
/* Macchine a stati interne                                                  */
/* ========================================================================= */

/* Energie pesate del frame: proporzioni 40% - 20% - 40% su 3 colonne. */
typedef struct {
  uint32_t left;
  uint32_t center;
  uint32_t right;
  uint32_t total;
} gesture_sums_t;

typedef enum {
  WAKE_IDLE,
  WAKE_WAIT_FIRST_PAUSE,
  WAKE_WAIT_SECOND_PEAK,
  WAKE_WAIT_FINAL_PAUSE
} wake_state_t;

typedef enum {
  ACTION_IDLE,
  ACTION_WAIT_LEFT_PAUSE,
  ACTION_WAIT_RIGHT_PAUSE,
  ACTION_COOLDOWN
} action_state_t;

/* ========================================================================= */
/* Stato del modulo                                                          */
/* ========================================================================= */

EVENTSOURCE_DECL(gesture_events);

static BaseSequentialStream *s_dbg = NULL;

/* Configurazione I2C (timing del bus). */
static const I2CConfig s_i2ccfg = {
    /* 72MHz/9 = 8MHz I2CCLK. */
    STM32_TIMINGR_PRESC(8U) | STM32_TIMINGR_SCLDEL(3U) |
        STM32_TIMINGR_SDADEL(3U) | STM32_TIMINGR_SCLH(3U) |
        STM32_TIMINGR_SCLL(9U),
    0, 0};

/* Piattaforma del driver VL53L7CX (bus, config e indirizzo). */
static const VL53L7CX_Platform s_platform_cfg = {
    .address = GESTURE_I2C_ADDRESS,
    .i2cp    = GESTURE_I2C_BUS,
    .i2ccfg  = &s_i2ccfg,
};

/* Variabili del sensore (strutture grandi: statiche, non sullo stack). */
static VL53L7CX_Configuration      s_dev;         /* Configurazione sensore. */
static VL53L7CX_ResultsData        s_results;     /* Risultati di ranging. */
static VL53L7CX_Motion_Configuration s_motion_cfg; /* Config. motion. */

/* Stato delle due macchine a stati. */
static wake_state_t  s_wake_state   = WAKE_IDLE;
static action_state_t s_action_state = ACTION_IDLE;
static int           s_wake_timeout   = 0;
static int           s_action_timeout = 0;
static int           s_cooldown       = 0;
static uint8_t       s_active         = 0; /* 1 = sistema ATTIVO. */

/* ========================================================================= */
/* Calcolo delle energie per zona (40% - 20% - 40%)                          */
/* ========================================================================= */

/*
 * Pesi colonna (per ogni riga della griglia 4x4) che danno le proporzioni
 * 40% - 20% - 40% per una mano che copre tutta la larghezza:
 *   col 0 -> sinistra, col 3 -> destra, col 1/2 -> divise tra centro e lato.
 * Accumulo in 64 bit per evitare overflow sui picchi piu' estremi; la
 * divisione per 10 finale non cambia i valori validati su hardware.
 */
static void gesture_sums(const uint32_t *motion,
                         gesture_sums_t *out) {

  uint64_t left = 0, center = 0, right = 0, total = 0;
  uint8_t i;

  for (i = 0; i < GESTURE_ZONES; i++) {
    uint64_t val = motion[i];
    uint8_t col = i % GESTURE_COLS;

    total += val;
    if (col == 0) {
      left += val * 10U;
    }
    else if (col == 1) {
      left += val * 6U;
      center += val * 4U;
    }
    else if (col == 2) {
      center += val * 4U;
      right += val * 6U;
    }
    else {
      right += val * 10U;
    }
  }

  out->left   = (uint32_t)(left / 10U);
  out->center = (uint32_t)(center / 10U);
  out->right  = (uint32_t)(right / 10U);
  out->total  = (uint32_t)total;
}

/* ========================================================================= */
/* FSM di wake-up (sistema SPENTO)                                           */
/* ========================================================================= */

/*
 * Attende il doppio movimento: primo picco -> mano ferma -> secondo picco
 * -> mano ferma. Se la sequenza non si completa entro WAKE_TIMEOUT_FRAMES
 * torna in WAKE_IDLE. Al completamento porta il sistema ATTIVO e pubblica
 * EVT_GESTURE_DOUBLE_WAVE.
 */
static void wake_process(const gesture_sums_t *sums) {

  /* Timeout della sequenza: un frame alla volta. */
  if (s_wake_state != WAKE_IDLE) {
    s_wake_timeout--;
    if (s_wake_timeout <= 0) {
      s_wake_state = WAKE_IDLE;
      chprintf(s_dbg, "[Wake-up] Timeout, riprova.\r\n");
    }
  }

  switch (s_wake_state) {
    case WAKE_IDLE:
      /* Primo movimento della mano. */
      if (sums->total > WAKE_PEAK_TOTAL) {
        s_wake_state = WAKE_WAIT_FIRST_PAUSE;
        s_wake_timeout = WAKE_TIMEOUT_FRAMES;
        chprintf(s_dbg, "[Wake-up] 1/4 - Primo movimento (Tot: %lu)\r\n",
                 sums->total);
      }
      break;

    case WAKE_WAIT_FIRST_PAUSE:
      /* Mano ferma tra il primo e il secondo movimento. */
      if (sums->total < WAKE_PAUSE_TOTAL) {
        s_wake_state = WAKE_WAIT_SECOND_PEAK;
        chprintf(s_dbg, "[Wake-up] 2/4 - Mano ferma...\r\n");
      }
      break;

    case WAKE_WAIT_SECOND_PEAK:
      /* Secondo movimento della mano. */
      if (sums->total > WAKE_PEAK_TOTAL) {
        s_wake_state = WAKE_WAIT_FINAL_PAUSE;
        chprintf(s_dbg, "[Wake-up] 3/4 - Secondo movimento (Tot: %lu)\r\n",
                 sums->total);
      }
      break;

    case WAKE_WAIT_FINAL_PAUSE:
      /* Mano ferma: sequenza completata, il sistema si attiva. */
      if (sums->total < WAKE_PAUSE_TOTAL) {
        s_wake_state = WAKE_IDLE;
        s_active = 1;
        s_action_state = ACTION_COOLDOWN;
        s_cooldown = COOLDOWN_FRAMES;
        chEvtBroadcastFlags(&gesture_events, EVT_GESTURE_DOUBLE_WAVE);
      }
      break;

    default:
      s_wake_state = WAKE_IDLE;
      break;
  }
}

/* ========================================================================= */
/* FSM delle azioni laterali (sistema ATTIVO)                                */
/* ========================================================================= */

/*
 * Attende un picco di motion dominante su una zona laterale e poi il
 * ritorno della mano (zona sotto la soglia di pausa). Al completamento
 * pubblica EVT_GESTURE_LEFT o EVT_GESTURE_RIGHT.
 */
static void action_process(const gesture_sums_t *sums) {

  /* Pausa post-evento: ignora tutto per COOLDOWN_FRAMES frame. */
  if (s_action_state == ACTION_COOLDOWN) {
    if (--s_cooldown <= 0) {
      s_action_state = ACTION_IDLE;
    }
    return;
  }

  /* Timeout dell'azione in corso: un frame alla volta. */
  if (s_action_state != ACTION_IDLE) {
    s_action_timeout--;
    if (s_action_timeout <= 0) {
      s_action_state = ACTION_IDLE;
    }
  }

  switch (s_action_state) {
    case ACTION_IDLE:
      if ((sums->left > ACTION_PEAK_ZONE) &&
          (sums->left > sums->center) && (sums->left > sums->right)) {
        s_action_state = ACTION_WAIT_LEFT_PAUSE;
        s_action_timeout = ACTION_TIMEOUT_FRAMES;
      }
      else if ((sums->right > ACTION_PEAK_ZONE) &&
               (sums->right > sums->center) && (sums->right > sums->left)) {
        s_action_state = ACTION_WAIT_RIGHT_PAUSE;
        s_action_timeout = ACTION_TIMEOUT_FRAMES;
      }
      break;

    case ACTION_WAIT_LEFT_PAUSE:
      /* Mano rientrata: azione sinistra completata. */
      if (sums->left < ACTION_PAUSE_ZONE) {
        s_action_state = ACTION_COOLDOWN;
        s_cooldown = COOLDOWN_FRAMES;
        chEvtBroadcastFlags(&gesture_events, EVT_GESTURE_LEFT);
        s_active = 0;
      }
      break;

    case ACTION_WAIT_RIGHT_PAUSE:
      /* Mano rientrata: azione destra completata. */
      if (sums->right < ACTION_PAUSE_ZONE) {
        s_action_state = ACTION_COOLDOWN;
        s_cooldown = COOLDOWN_FRAMES;
        chEvtBroadcastFlags(&gesture_events, EVT_GESTURE_RIGHT);
        s_active = 0;
      }
      break;

    default:
      s_action_state = ACTION_IDLE;
      break;
  }
}

/* ========================================================================= */
/* Riconoscimento di un frame di motion                                      */
/* ========================================================================= */

static void gesture_process(const uint32_t *motion) {

  gesture_sums_t sums;

  /* 1. Energie pesate del frame (40% - 20% - 40%). */
  gesture_sums(motion, &sums);

  /* 2. A sistema SPENTO si ascolta solo il doppio movimento (wake-up). */
  if (!s_active) {
    wake_process(&sums);
    return;
  }

  /* 3. A sistema ATTIVO si ascoltano le azioni laterali. */
  action_process(&sums);
}

/* ========================================================================= */
/* Inizializzazione del sensore VL53L7CX                                     */
/* ========================================================================= */

/*
 * Sequenza di avvio del sensore. Restituisce true se tutto e' andato a
 * buon fine. Da eseguire in contesto thread (usa sleep).
 */
static bool sensor_start(void) {

  uint8_t status;
  uint8_t is_alive;

  VL53L7CX_WaitMs(&s_dev.platform, 100);

  status = vl53l7cx_is_alive(&s_dev, &is_alive);
  if ((status != 0U) || (is_alive == 0U)) {
    chprintf(s_dbg, "VL53L7CX not detected at requested address\r\n");
    return false;
  }

  status = vl53l7cx_init(&s_dev);
  if (status != 0U) {
    chprintf(s_dbg, "VL53L7CX ULD Loading failed\r\n");
    return false;
  }
  chprintf(s_dbg, "VL53L7CX ULD ready ! (Version : %s)\r\n",
           VL53L7CX_API_REVISION);

  status = vl53l7cx_motion_indicator_init(&s_dev, &s_motion_cfg,
                                          VL53L7CX_RESOLUTION_4X4);
  if (status != 0U) {
    chprintf(s_dbg,
             "Motion indicator init failed with status : %u\r\n", status);
    return false;
  }

  status = vl53l7cx_motion_indicator_set_distance_motion(&s_dev,
                                                         &s_motion_cfg,
                                                         400, 1000);
  if (status != 0U) {
    chprintf(s_dbg,
             "Motion indicator set distance motion failed with status : %u\r\n",
             status);
    return false;
  }

  status = vl53l7cx_set_ranging_frequency_hz(&s_dev, GESTURE_RANGING_HZ);
  if (status != 0U) {
    chprintf(s_dbg,
             "vl53l7cx_set_ranging_frequency_hz failed, status %u\r\n",
             status);
    return false;
  }

  status = vl53l7cx_start_ranging(&s_dev);
  if (status != 0U) {
    chprintf(s_dbg, "vl53l7cx_start_ranging failed, status %u\r\n", status);
    return false;
  }

  return true;
}

/* ========================================================================= */
/* Thread interno: acquisizione + riconoscimento + publish                   */
/* ========================================================================= */

static THD_WORKING_AREA(wa_gesture, 1024);
static THD_FUNCTION(gesture_thread, arg) {

  (void)arg;
  chRegSetThreadName("gesture");

  /* Piattaforma (bus I2C) del sensore. */
  s_dev.platform = s_platform_cfg;

  if (!sensor_start()) {
    /* Errore di init: come nella versione monolitica il programma si
       ferma qui; gli altri thread (demo) continuano a girare. */
    while (true) {
      chThdSleepMilliseconds(1000);
    }
  }

  while (true) {
    uint8_t is_ready = 0;

    (void)vl53l7cx_check_data_ready(&s_dev, &is_ready);

    if (is_ready != 0U) {
      uint32_t motion[GESTURE_ZONES];
      uint8_t i;

      vl53l7cx_get_ranging_data(&s_dev, &s_results);

      /* Riordina i 16 valori di motion secondo la mappa del sensore. */
      for (i = 0; i < GESTURE_ZONES; i++) {
        motion[i] = s_results.motion_indicator.motion[s_motion_cfg.map_id[i]];
      }

      gesture_process(motion);
    }

    /* Ridotto a 20 ms per seguire meglio i 15 Hz di ranging. */
    VL53L7CX_WaitMs(&s_dev.platform, GESTURE_POLL_MS);
  }
}

/* ========================================================================= */
/* API pubblica                                                              */
/* ========================================================================= */

void gesture_init(BaseSequentialStream *dbg) {

  s_dbg = dbg;

  /* Linee I2C del sensore (SCL/SDA in alternate 4, open-drain, pull-up). */
  palSetLineMode(GESTURE_SCL_LINE,
                 PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN |
                     PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_PULLUP);
  palSetLineMode(GESTURE_SDA_LINE,
                 PAL_MODE_ALTERNATE(4) | PAL_STM32_OTYPE_OPENDRAIN |
                     PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_PULLUP);

  /* Thread di acquisizione/riconoscimento (publisher su gesture_events). */
  chThdCreateStatic(wa_gesture, sizeof(wa_gesture), NORMALPRIO,
                    gesture_thread, NULL);
}

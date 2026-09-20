# Rescue Vest - Develop

Struttura di sviluppo del progetto Rescue Vest. Le cartelle riprendono quelle
della versione finale su `main`; i segnaposto `.gitkeep` conservano in Git le
cartelle vuote.

```text
Rescue Vest - Develop/
├── comunication/
│   ├── client/
│   └── server/
├── rilevazione parametri vitali/
│   ├── PPG/
│   └── MQ2/
├── GPS/
│   └── gps-old/
└── gesture/
```

Anche le sottocartelle interne dei moduli sono predisposte vuote.
`GPS/gps-old/` contiene il progetto precedente
`RT-STM32G474RE-NUCLEO64-GNSS-V3` e lo script desktop `gnss_live_map.py`,
recuperati dal branch `gps` al commit
`88626c3215643d29d9c58c38d11195a30592bbf3`.
I sorgenti sono invariati; sono esclusi i due file launch del vecchio debug.

Il branch `gps` rimane disponibile con il contenuto originale. Il progetto
finale resta nel branch `main`.

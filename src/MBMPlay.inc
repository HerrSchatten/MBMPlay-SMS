;=================================================================================
; RAM
;=================================================================================

.def MBMIVLIST_OFFSET          $00A3
.def MBMSTARTINSTR_OFFSET      $0101
.def MBMOPLDATA_OFFSET         $010A
.def MBMDRUMBLOCK_OFFSET       $0148
.def MBMDRUMVOL_OFFSET         $0157
.def MBMDRUMFREQ_OFFSET        $015A
.def MBMSTARTDETUNE_OFFSET     $016E 

.ramsection "FM player variables" slot 3

MBMStatus          db               ; status: 0 = not playing, 255 = busy playing
MBMBank            db               ; music data bank
MBMStart           dw               ; music data address
MBMPosition        db               ; current position
MBMStep            db               ; current step
MBMStatusByte      dsb 3            ; 3 status bytes
MBMStepBuffer      dsb 13           ; step data to be played on the NEXT interrupt or has been played previously

; common data

MBMFrequency       db               ; frequency
MBMTransposeValue  db               ; transpose value
MBMSpeed           db               ; speed value
MBMSpeedCounter    db               ; speed counter
MBMPatternAddress  dw               ; Pattern data start
MBMPatternPointer  dw               ; current pattern line pointer
MBMPAT             dw               ; Current pattern address table
MBMFading          db               ; fading speed: if 0, no fading
MBMFadeCounter     db               ; fade counter
MBMChannelBuffer   dsb 9*14         ; channel buffer

; music settings

MBMIVListAddr      dw               ; instrument/volume list address
MBMDrumBlockAddr   dw               ; drum set-up address

MBMSongLength      db               ; song length (1 byte)
MBMChannelInfo     db               ; number of channels: bit 6 (0-7) on is 6chn+drm, off is 9chn
MBMLoopPoint       db               ; loop position


MBMLastCustInstr   db               ; last programmed custom instrument
.ends


;=================================================================================
; MoonBlaster 1.4 driver
; MB1.4 assembly sources written by Remco Schrijvers
; Extended by BiFi / The New Image 2012/2019
; Adapted for the SMS by Kagesan with help from Zipper and Calindro
;=================================================================================

; MBMPlay = music start
; MBMStop = music stop
; MBMResume = music continue

.section "start fm music" free

MBMPlay:

    ld a, (MBMStatus)               ; check if a song is already playing
    or a                            ; yes?
    call nz, MBMStop                ; then stop it

    xor a
    ld (MBMSpeedCounter), a         ; reset speed counter
    ld (MBMFading), a               ; reset fading

    dec a
    ld (MBMPosition), a             ; reset position
    ld (MBMStatus), a               ; set status to "playing"
    ld (MBMLastCustInstr),a         ;* reset last custom instrument

    ld a, (MBMBank)                 ; get bank the music is placed in
    ld (BankSwitch), a              ; switch bank
    
    ld hl,(MBMStart)
    ld de,MBMIVLIST_OFFSET
    add hl,de
    ld (MBMIVListAddr),hl
    ld de,MBMDRUMBLOCK_OFFSET-MBMIVLIST_OFFSET
    add hl,de
    ld (MBMDrumBlockAddr),hl

    ld hl, (MBMStart)               ; transfer first part of header
    ld de, MBMSongLength

    ldi                             ; transfer song length [1 byte]

    ld bc, 2+144+16+32+10           ; skip song ID, msxa voice data, msxa instrument list, fm instrument / default volume list, channel chip set
    add hl, bc

    ld a,(hl)                       ;
    ld (MBMSpeed),a                 ; initialize tempo
    inc hl                          ;
    
    ldi                             ; transfer channel number (bit 5 on is 6 channels, off is 9 channels) [1 byte]

    ld bc, 41+9+9+48+6+8+15+3+20+9  ; skip song name, msxa start instruments, fm start instruments (9), fm opl data (48), fm opl prog nr (6), sample kit name (8), drum set-up (15), drum volumes (3), drum frequencies (20), start detune (9)
    add hl, bc

    ldi                             ; transfer loop position [1 byte]

    ld (MBMPAT), hl                 ; set pointer to start pattern of song
    ld a, (MBMSongLength)           ; get song length
    inc a
    ld e, a
    ld d, 0
    add hl, de                      ; skip the song length to point to pattern address table
    ld (MBMPatternAddress), hl      ; store the address of the pattern data start

; set start detune and instruments (reversed?) -->

    ld b, 9
    ld de, MBMSTARTINSTR_OFFSET+8
    ld iy, (MBMStart)
    add iy,de

    ld ix,(MBMDrumBlockAddr)
    ld de,MBMSTARTDETUNE_OFFSET-MBMDRUMBLOCK_OFFSET
    add ix,de

    ld hl, MBMChannelBuffer+112+3
    ld de, -17                      ; skip through the channels reversed

-:  ld (hl), 0                      ; reset frequency mode
    inc hl                          ; skip frequency register
    inc hl                          ; move to start detune
    ld a, (ix+0)                    ; get start detune
    ld (hl), a                      ; set start detune
    inc hl                          ; move to start instrument
    ld a, (iy+0)                    ; get start instrument
    ld (hl), a                      ; set start instrument
    inc ix
    dec iy
    add hl, de
    djnz -

; initialize frequency registers -->

    ld hl, MBMChannelBuffer+4
    ld a, $10                       ; first channel frequency register
    ld de, 14
    ld b, 9

-:  ld (hl), a
    inc a
    add hl, de
    djnz -

; initialize volume registers -->

    ld hl, MBMChannelBuffer+7
    ld a, $30                       ; first channel volume register
    ld de, 14
    ld b, 9

-:  ld (hl), a
    inc a
    add hl, de
    djnz -

    call _InitInstr                 ; initialize instruments

    ld a, 15
    ld (MBMStep), a

    ld a, 48
    ld (MBMTransposeValue), a
    
    ld a, (MBMSpeed)                ; skip leading silence
    dec a
    ld (MBMSpeedCounter), a         
    
    jp MBMFrame                     ; refresh step buffer, end setup

; initialize instruments

_InitInstr:

    ld hl,(MBMStart)
    ld de,MBMSTARTINSTR_OFFSET
    add hl,de

    ld iy, MBMChannelBuffer

    ld b, 6

    ld a, (MBMChannelInfo)          ; check if 6 or 9 channels
    bit 5, a

    push af

    jr nz, +                        ; if 6 channels, continue

    ld b, 9                         ; else set 9 channels

+:  ld c, 48                        ; first register to write to

-:  push bc
    push hl

    ld a, (hl)
    ld hl, (MBMIVListAddr)
    dec a
    
    add a, a
    ld c, a
    ld b, 0
    add hl, bc                      ; hl = adress of instrument/volume pair

    ld a, (hl)                      ; get start instrument
    cp 16                           ; check if hardware instrument or original
    call nc, _InitOPL               ; if instr number >= 16, then it's a software instrument --> set it up
    rlca                            ; get instrument number into upper nibble
    rlca
    rlca
    rlca
    inc hl

    ld b, (hl)                      ; get start volume
    add a, b                        ; combine with instrument
    rlc b
    rlc b
    ld (iy+13), b                   ; write FM volume
    ld bc, 14
    add iy, bc                      ; move to next channel

    pop hl
    pop bc

    call FMOut ;*

    inc hl                          ; next start instrument
    inc c                           ; next register to write to

    djnz -                          ; repeat 6 or 9 times until setup is finished

; silence drums -->

    xor a
    ld c, 14
    
            ;* 75
    ret nz  ;*  (always false, wait 5 cycles)
    nop     ;*

    call FMOut

    pop af                          ; restore flags for test 9 or 6 fm channels

    ret z                           ; ret if 9 channels

; drums setup

    ld hl,(MBMDrumBlockAddr)
    ld de,MBMDRUMVOL_OFFSET-MBMDRUMBLOCK_OFFSET
    add hl,de
    push hl
    ex de,hl
    
    ld hl, MBMDrumReg
    ld b, 9

-:  ld c, (hl)                      ; get register
    ld a, (de)                      ; get data

    call FMOut ;*
                   ;* 85

    inc hl
    inc de

    djnz -
    
    pop hl
    ld a,(hl)
    and %00001111
    rlca
    rlca
    ld (MBMChannelBuffer+(6*14)+13), a
    inc hl
    ld a,(hl)
    and %11110000
    rrca
    rrca
    ld (MBMChannelBuffer+(7*14)+13), a
    inc hl
    ld a,(hl)
    and %00001111
    rlca
    rlca
    ld (MBMChannelBuffer+(8*14)+13), a

    ret

_InitOPL:

    push hl

    sub 16                          ; get number of original instrument
    
    ld hl,MBMLastCustInstr          ; avoid setting same instrument again
    cp (hl)
    call nz,MBMReprogramCustomInstr ; if not same instrument, reprogram chip
    
    pop hl                          ; revert to instr/vol pair list position

    xor a                           ; instrument number of original instrument is 0

    ret

.ends

.section "Continue FM music" free

MBMResume:

  ld a, (MBMStatus)                 ; check if a song is already playing
  or a                              ; yes?
  ret nz                            ; then do nothing, return

  dec a
  ld (MBMStatus), a                 ; set status to "playing"

  ret

.ends

.section "FM music interrupt routine" free

MBMFrame:

    ld a, (MBMStatus)               ; check if a song is already playing
    or a                            ; no?
    jp z, MBMSilenceChannels        ; then silence all channels

    ld a,(MBMBank)                  ; switch to bank with music data
    ld (BankSwitch),a

    ld a, (MBMFading)               ; check for fade
    or a
    call nz, MBMDoFade              ; do fade if speed value is > 0

    call MBMDoPitchBend             ; pitch bend

    ld a, (MBMSpeed)                ; handle speed
    ld hl, MBMSpeedCounter
    inc (hl)
    cp (hl)
    jp nz, _NoStep

; process music data

    ld (hl), 0                      ; reset speed counter
    ld iy, MBMChannelBuffer
    ld hl, MBMStepBuffer
    ld b, 9
    
    ld a, (MBMSFXStatus)            ; check if sfx is playing
    ld d, a

-:  push bc                         ; store channel counter

    ld a, (hl)                      ; get command of channel
    or a
    jp z, ++                        ; no command ?

    push hl                         ; store pointer to play buffer

    cp 97
    jp c, _OnEvent                  ; < 97 is a note
    jp z, _OffEvent                 ; 97 = silence
    cp 114
    jp c, _ChangeInstrEvent         ; change instrument
    cp 177
    jp c, _ChangeVolumeEvent        ; change volume
    cp 180
    jp c, _ContinueFrame            ; skip command, was -> chgste (stereo)
    cp 199
    jp c, _LinkEvent                ; link event        (180 -> L-9, 189 -> L+0, 198 -> L+9)
    cp 218
    jp c, _ChangePitchEvent         ; change pitch bend (199 -> P-9, 208 -> P+0, 217 -> P+9)
    cp 224
    jp c, _ChangeBrightnessNegEvent ; change brightness (negative)
    cp 231
    jp c, _ChangeDetuneEvent        ; change detune     (224 -> T-3, 227 -> T+0, 230 -> T+3)
    cp 237
    jp c, _ChangeBrightnessPosEvent ; change brightness (positive)
    cp 238
    jp c, _ChangeSustainEvent       ; change sustain
    jp _ChangeModulationEvent       ; change modulation

_ContinueFrame:

    pop hl

++: ld bc, 14
    add iy, bc                      ; point to next channel data
    inc hl                          ; next channel of step buffer

    pop bc                          ; restore channel counter
    
    ld d, 0                         ; reset sfx flag for all channels following channel 1

    djnz -                          ; loop all 9 channels

    inc hl                          ; skip unused MSX-Audio channels
    inc hl

    call MBMPlayDrum

 ; process replayer command

    inc hl
    ld a, (hl)
    or a
    ret z                           ; no command
    cp 24
    jp c, MBMChangeTempo            ; command: change tempo
    jp z, MBMEndOfPattern           ; command: end of pattern
    cp 28
    jp c, MBMChangeDrumset          ; command: change drumset
    cp 31
    jp c, MBMChangeStatusByte       ; command: change status bytes

    jp MBMChangeTransposeValue      ; command: transpose

; check if there's need to prepare step buffer for next isr call

_NoStep:

    dec a
    cp (hl)
    ret nz
    ld a, (MBMStep)
    inc a
    and %00001111                   ; wrap around 16 (0-15)
    ld (MBMStep), a
    ld hl, (MBMPatternPointer)      ; pattern pointer
    call z, MBMNextPattern          ; next pattern
    ld de, MBMStepBuffer            ; decrunch data
    ld c, 13

-:  ld a, (hl)
    cp 243
    jr nc, +                        ; contains crunched data
    ld (de), a
    inc de
    dec c

--: inc hl                          ; check if all 13 bytes are processed
    ld a, c
    or a
    jr nz, -

; end of loop

    ld (MBMPatternPointer), hl      ; store pattern line pointer
    jp ++

+:  sub 242                         ; process crunched data
    ld b, a
    xor a

-:  ld (de), a
    inc de
    dec c
    djnz -

    jr --

++: ld iy, MBMChannelBuffer
    ld hl, MBMStepBuffer
    ld b, 9

-:  push bc

    ld a, (hl)
    or a
    jr z, +
    cp 97
    jr nc, +

    push hl

    ld (iy+3), 0                    ; reset frequency mode
    ld c, a

    push bc
    push af

    call _FinishUp                  ; call nz, pacple

    pop af
    pop bc

    pop hl

+:  ld bc, 14                       ; move to next channel
    add iy, bc

    inc hl

    pop bc

    djnz -

    ret

_FinishUp:

    ld a, (MBMTransposeValue)       ; transpose
    add a, c
    cp 96+48+1
    jr c, +
    sub 96

    jr ++

+:  cp 1+48
    jr nc, ++
    add a, 96

++: sub 48
    ld (iy+2), a                    ; note link
    ld hl, MBMFreqencyLUT-1
    add a, a
    add a, l
    ld l, a
    jr nc, +

    inc h

+:  ld a, (hl)
    ld (iy+10), a                   ; original frequency
    dec hl
    ld a, (hl)
    add a, (iy+5)                   ; tuning
    ld (iy+11), a                   ; original frequency

ret

_OnEvent:                           ; on event

    call MBMKeyOn                   ; call nz, pacpl

    jp _ContinueFrame

_OffEvent:                          ; off event

    ld (iy+3), 0                    ; reset frequency mode

    call MBMKeyOff                  ; call nz, offpap

    jp _ContinueFrame

_ChangeSustainEvent:                ; change sustain

    ld (iy+3), 0                    ; reset frequency mode

    call MBMKeyOffSustained         ; call nz, suspap

    jp _ContinueFrame

_ChangeInstrEvent:                  ; change instrument

    ld (iy+3), 0                    ; reset frequency mode
    sub 97
    ld c, a

    call MBMChangeInstrument        ; call nz, chpaci

    jp _ContinueFrame

_ChangeVolumeEvent:                 ; change volume

    sub 114
    ld c, a

    call MBMChangeVolume            ; call nz, chpacv

    jp _ContinueFrame

_LinkEvent:                         ; link event

    sub 189
    ld c, a

    call MBMLinkNote                ; call nz, chlkpa

    jp _ContinueFrame

_ChangePitchEvent:                  ; change pitch bend

    sub 208
    ld (iy+3), 1                    ; set frequency mode: pitch
    ld (iy+8), a                    ; pitch value
    rlca
    jr c, +
    ld (iy+9), 0                    ; set pitch value 0 = positive

    jp _ContinueFrame

+:  ld (iy+9), $FF                  ; set pitch value -1 = negativ

    call MBMChangePitch             ; call nz, chpidp

    jp _ContinueFrame

_ChangeBrightnessNegEvent:          ; change brightness (negative)

    sub 224

    jr _ChangeBrightness

_ChangeBrightnessPosEvent:          ; change brightness (positive)

    sub 230

_ChangeBrightness:

    push de

    ld c, a

    call MBMChangeBrightness        ; call nz, chpcbr

    pop de

    jp _ContinueFrame

_ChangeDetuneEvent:                 ; change detune

    sub 227
    ld (iy+5), a                    ; write detune

    jp _ContinueFrame

_ChangeModulationEvent:             ; change modulation

    ld (iy+3), 2                    ; set frequency mode: modulation

    jp _ContinueFrame

MBMNextPattern:                     ; next pattern

    ld a, (MBMSongLength)
    ld b, a
    ld a, (MBMPosition)
    cp b

    jr nz, _Next

    ld a, (MBMLoopPoint)
    cp 255

    jr z, +

    dec a

_Next:

    inc a
    ld (MBMPosition), a
    ld c, a
    ld b, 0
    ld hl, (MBMPAT)
    add hl, bc
    ld a, (hl)
    dec a
    add a, a
    ld c, a
    ld hl, (MBMPatternAddress)
    add hl, bc
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ld de, (MBMStart)
    add hl, de

    ret

+:  xor a
    ld (MBMStatus), a
    dec a

    jr _Next

.ends

.section "FM routines" free

; note on event -->

MBMKeyOn:

    ld a, (iy+11)                   ; read original frequency
    ld c, (iy+4)                    ; get register
    bit 0, d                        ; check if sfx is playing
    call z, FMOut
    ld (iy+0), a                    ; write frequency
    ld a, c
    add a, $10
    ld c, a
    ld a, (iy+10)                   ; read original frequency
    bit 0, d                        ; check if sfx is playing
    call z, FMOut
    set 4, a
    ld (iy+1), a                    ; write  frequency
    bit 0, d                        ; check if sfx is playing
    ret nz
           ;* 79
    ret nz ;* always false, here just to wait 5 cycles
    
    jp FMOut

; note off event (sustained) -->

MBMKeyOffSustained:

    ld l, %00100000
    jr +

; note off event -->

MBMKeyOff:

    ld l, 0

+:  ld c, (iy+4)                    ; get register
    ld a, (iy+0)                    ; read frequency
    bit 0, d                        ; check if sfx is playing
    call z, FMOut
    ld a, c
    add a, $10                      ; next register
    ld c, a
    ld a, (iy+1)                    ; read frequency
    and %11101111
    or l
    ld (iy+1), a                    ; write frequency
    bit 0, d                        ; check if sfx is playing
    jp z, FMOut

    ret

; change instrument -->

MBMChangeInstrument:

    ld a, c
    ld (iy+6), a                    ; write instrument
    dec a
    add a, a
    ld c, a
    ld b, 0
    ld hl,(MBMIVListAddr)
    
    add hl, bc
    ld a, (hl)                      ; get instrument
    cp 16                           ; check if original instrument
    jp nc, MBMChangeOriginalInstr   ; if yes, set it up
    rlca                            ; get instrument number in upper nibble
    rlca
    rlca
    rlca

_ChangeInstrCont:

    inc hl
    ld c, (hl)                      ; get volume
    ld l, a
    add a, c                        ; combine instrument and volume

    push bc

    ld c, (iy+7)                    ; read volume register
    bit 0, d                        ; check if sfx is playing
    call z, FMOut

    pop bc

    rlc c
    rlc c
    ld b, (iy+13)                   ; get previous volume
    ld (iy+13), c                   ; store the new volume
    ld a, (MBMFading)
    or a
    ret z                           ; return if fading is not active

    ld a, b
    cp c
    ret c                           ; return if old and new volume are smaller

    ld (iy+13), b
    srl b
    srl b
    ld a, l                         ; l contained the instrument value
    add a, b
    ld c, (iy+7)
    bit 0, d                        ; check if sfx is playing
    jp z, FMOut                 ; write the changed volume to the FM registers
    
    ret

MBMChangeOriginalInstr:

    exx
    sub 16
    
    ld hl,MBMLastCustInstr          ; avoid setting same instrument again
    cp (hl)
    call nz,MBMReprogramCustomInstr ; if not same instrument, reprogram chip

    exx
    xor a
    jp _ChangeInstrCont

; change volume -->

MBMChangeVolume:

    ld a, c

    push af

    srl a
    srl a
    ex af, af'

    ld a, (iy+6)                    ; get instrument
    ld b, 0
    dec a
    add a, a
    ld c, a
    ld hl,(MBMIVListAddr)
    add hl, bc
    ld a, (hl)
    cp 16
    jr c, +
    xor a

+:  rlca
    rlca
    rlca
    rlca
    ld b, a
    ld c, (iy+7)
    ex af, af'

    xor b
    bit 0, d                        ; check if sfx is playing
    call z, FMOut
    ld l, b

    pop bc

    ld c, (iy+13)                   ; read volume
    ld (iy+13), b                   ; write volume
    ld a, (MBMFading)
    or a
    ret z

    ld a, c
    cp b
    ret c

    ld (iy+13), c
    ld a, c
    srl a
    srl a
    xor l
    ld c, (iy+7)
    bit 0, d                        ; check if sfx is playing
    jp z, FMOut
    
    ret

; note linking, legato -->

MBMLinkNote:

    ld a, (iy+2)
    add a, c
    ld (iy+2), a

    ld hl, MBMFreqencyLUT-1
    add a, a
    add a, l
    ld l, a
    jr nc, +
    inc h

+:  ld a, (hl)
    ld (MBMFrequency), a
    dec hl
    ld a, (hl)                      ; get frequency
    add a, (iy+5)                   ; add detune
    ld (iy+0), a                    ; write new frequency
    ld c, (iy+4)                    ; get register
    bit 0, d                        ; check if sfx is playing
    call z, FMOut
    ld a, c
    add a, 010h                     ; next register
    ld c, a
    ld a, (MBMFrequency)
    or %00010000
    ld (iy+1), a                    ; write new frequency
    ld (iy+3), 0                    ; reset frequency mode
    bit 0, d                        ; check if sfx is playing
    jp z, FMOut
    
    ret

; change brightness -->

MBMChangeBrightness:

    ld a, (iy+6)                    ; get instrument
    dec a
    add a, a
    ld e, a
    ld d, 0
    ld hl, (MBMIVListAddr)
    add hl, de
    ld a, (hl)
    cp 16                           ; is instrument original?
    ret c                           ; no? then no brightness change possible

    ld a, (iy+12)
    and %11000000
    ld e, a
    ld a, (iy+12)
    and %00111111
    ld b, a
    ld a, c
    add a, b
    add a, e
    ld (iy+12), a
    ld c, 2
    jp FMOut

; change pitch bend -->

MBMChangePitch:

    ld a, (iy+1)
    bit 0, a
    ret nz

    dec a
    ld (iy+1), a
    ld a, (iy+0)
    add a, a
    ld (iy+0), a

    ret

; drums -->

MBMPlayDrum:

    ld a, (hl)
    and %00001111
    or a
    ret z

    dec a
    ld e, a
    ld d, 0

    push hl

    ld hl,(MBMDrumBlockAddr)
    add hl, de
    ld a, (hl)
    ld c, a

    pop hl

    ld a, (MBMChannelInfo)
    and %00100000
    ret z

    ld a, c
    and %00011111
    ld c, $0E
    call FMOut ;*
    
          ;* 47
    sub 0 ;*
    sub 0 ;*
    sub 0 ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*

    set 5, a
    jp FMOut

; command routines -->

MBMChangeTempo:                     ; change tempo

    ld b, a
    ld a, 25
    sub b
    ld (MBMSpeed), a
    ret

MBMEndOfPattern:                    ; end of pattern

    ld a, 15
    ld (MBMStep), a
    ret

MBMChangeDrumset:                   ; change drumset

    sub 25
    add a, a
    ld b, a
    add a, a
    add a, b
    ld e, a
    ld d, 0
    ld hl,(MBMDrumBlockAddr)
    ld bc,MBMDRUMFREQ_OFFSET-MBMDRUMBLOCK_OFFSET
    add hl,bc
    add hl, de
    ex de, hl
    ld hl, MBMDrumReg+3
    ld b, 6

-:  ld c, (hl)
    ld a, (de)
    call FMOut
    inc hl
    inc de
    djnz -

    ret

; status bytes -->

MBMChangeStatusByte:

    ld c, a
    ld b, 0
    ld hl, MBMStatusByte-28
    add hl, bc
    ld (hl), 255

    ret

; change transpose -->

MBMChangeTransposeValue:

    sub 55-48
    ld (MBMTransposeValue), a

    ret
    
; reprogram OPL custom instrument

MBMReprogramCustomInstr:

    ld (hl),a                       ; save instrument number to avoid setting same instrument again later
    
    add a,a
    add a,a
    add a,a
    add a, <MBMOPLDATA_OFFSET
    ld c, a
    ld b, >MBMOPLDATA_OFFSET    
    ld hl,(MBMStart)
    add hl, bc                      ; move to OPL data start address of original instrument

    push hl                         ; save start address

    inc hl
    inc hl
    ld a, (hl)                      ; get instrument
    ld (iy+12), a                   ; write instrument

    pop hl                          ; revert to OPL data start adress

    ld bc, $0800                    ; 8 registers, starting from $00

-:  ld a, (hl)                      ; get OPL data
    call FMOut
    
                                    ; cycles = 76
    nop                             ;
    nop                             ; +8 = 84
    
    inc c                           ; next register
    inc hl                          ; next data byte
    djnz -                          ; repeat 8 times

    ret

.ends

.section "FM interrupt routines" free

; 'every interrupt' routines -->

; music fade -->

MBMDoFade:                          ; decrease fade timer

    ld b, a
    ld a, (MBMFadeCounter)
    inc a
    ld (MBMFadeCounter), a
    cp b
    ret nz                          ; return until need to fade next step

    xor a
    ld (MBMFadeCounter), a
    ld iy, MBMChannelBuffer
    ld b, 9
    ld hl, 0
    
    ld a, (MBMSFXStatus)            ; check if sfx is playing
    ld ixl, a

_LoopThroughChannels:

    push bc

    ld a, b
    cp 3
    jr nc, +
    ld a, (MBMChannelInfo)
    bit 5, a                        ; check if 6 or 9 channels
    jp nz, _FadeDrums               ; 6ch mode

+:  push hl                         ; get instrument

    ld a, (iy+6)
    dec a
    add a, a
    ld c, a
    ld b, 0
    ld hl, (MBMIVListAddr)
    add hl, bc
    ld a, (hl)
    cp 16                           ; check if original instrument
    jr c, _FadeChannel
    xor a

_FadeChannel:

    rlca                            ; get instrument number in upper nibble
    rlca
    rlca
    rlca
    ld b, a
    ld a, (iy+13)                   ; get volume
    add a, 4
    cp 64
    jr c, +
    ld a, 63

+:  ld (iy+13), a                   ; write volume
    srl a
    srl a
    ld c, (iy+7)                    ; get volume register
    add a, b                        ; combine volume and instrument
    dec ixl                         ; check if sfx is playing
    call nz, FMOut

    pop hl

    and %00001111
    xor 15
    ld e, a
    ld d, 0
    add hl, de
    ld bc, 14
    add iy, bc                      ; move to next channel

    pop bc

    ld ixl, 0                       ; reset sfx flag

    dec b
    jp nz, _LoopThroughChannels

;*    ld de, $0000
;*
;*    ld a, h                         ; fading done?
;*    sub d
;*    ret nz
;*
;*    ld a, l
;*    sub e
;*    ret nz

    ld a,h                          ;* fading done?
    or l                            ;*
    ret nz                          ;*

;*    xor a                         ;* useless
    ld (MBMStatus), a               ; set status to 'not playing'
    ret

_FadeDrums:                         ; fade drums

    push hl
    ld a, (iy+13)                   ; get volume
    srl a
    srl a
    jp _FadeChannel

; pitch bending -->

MBMDoPitchBend:

    ld iy, MBMChannelBuffer

    ld a, (MBMSFXStatus)            ; check if sfx is playing
    ld ixl, a

    exx
    ld b, 9
    ld de, 14

-:  exx
    ld a, (iy+3)                    ; get frequency mode
    ld h, a
    or a                            ; if not zero (0 = normal)
    call nz, _Pitch                 ; do pitch bend

    exx
    add iy, de                      ; move to next channel
    ld ixl, 0                       ; reset sfx flag
    djnz -

    ret

_Pitch:

    dec h
    jr nz, _Modulate
    ld l, (iy+8)                    ; pitch mode
    ld h, (iy+9)                    ; pitch value
    ld c, (iy+0)                    ; frequency
    ld b, (iy+1)                    ; frequency
    bit 7, h
    jr nz, +
    add hl, bc
    ld a, b
    and %00000001
    ld b, a

_PitchLoop:

    ld (iy+0), l                    ; write frequency
    ld c, (iy+4)                    ; get frequency register
    ld a, l
    dec ixl
    call nz, FMOut
    ld a, c
    add a, $10                      ; next register
    ld c, a
    ld a, h
    or b
    ld (iy+1), a                    ; write frequency
    inc ixl 
    jp z, FMOut

    ret

+:  add hl, bc
    bit 0, h
    jr nz, +
    dec h

+:  ld b, 0
    jp _PitchLoop

; modulate -->

_Modulate:

    ld a, h
    add a, 2
    cp 12
    jr nz, +
    ld a, 2

+:  ld (iy+3), a                    ; set frequency mode: modulation

    ld a, h
    add a, a
    ld c, a
    ld b, 0
    ld hl, MBMModVal-2
    add hl, bc
    ld c, (hl)
    inc hl
    ld b, (hl)
    ld l, (iy+0)                    ; get frequency
    ld h, (iy+1)                    ; get frequency
    add hl, bc
    ld b, 0
    jp _PitchLoop

.ends

.section "FM out routine" free

FMOut:
;* C contains FM register #, A contains data

    ; to call here / to jp here:   17/10
    ex af, af'                 ;     4
    ld a, c                    ;     4
    out ($F0), a               ;    11
    ex af, af'                 ;     -
    out ($F1), a               ;     -
    ret                        ;    10
                        ; total =  46/39
.ends

.section "Stop FM" free

;------ music stop ------

MBMStop:

    ld a, (MBMStatus)               ; check if a song is already playing
    or a                            ; no?

    ret z                           ; then just return

MBMSilenceChannels:

    ld de, 14
    ld iy, MBMChannelBuffer
    ld b, 9
    
    ld a, (MBMSFXStatus)            ; check if sfx is playing
    ld ixl, a

-:  call _MSXOut
    add iy, de                      ; move to next channel
    ld ixl, 0                       ; reset sfx flag
    djnz -

    xor a
    ld (MBMStatus), a               ; set status to 'not playing'

    ret

_MSXOut:

    ld a, (MBMChannelInfo)          ; check if 6 or 9 channels
    bit 5, a
    jr z, +
    ld a, b
    cp 4

    ret c

+:  ld c, (iy+4)                    ; read frequency register
    xor a
    dec ixl                         ; check if sfx is playing
    call nz, FMOut ;*
    ld a, c                         ; set volume register
    add a, $10
    ld c, a
    xor a                           ; set key to off
    inc ixl                         ; check if sfx is playing

    ret nz
           ;* 71
    sub 0  ;*
    sub 0  ;*
    
    jp FMOut

.ends

.section "MBM cancel loop" free

MBMPlayNoRepeat:

    call MBMPlay
    
MBMCancelLoop:

    ld a, $FF
    ld (MBMLoopPoint), a

    ret

.ends

.section "MSX-MUSIC data" free

; MSX-MUSIC data -->

MBMFreqencyLUT:
.db $AD $00 $B7 $00 $C2 $00 $CD $00 $D9 $00 $E6 $00
.db $F4 $00 $03 $01 $12 $01 $22 $01 $34 $01 $46 $01
.db $AD $02 $B7 $02 $C2 $02 $CD $02 $D9 $02 $E6 $02
.db $F4 $02 $03 $03 $12 $03 $22 $03 $34 $03 $46 $03
.db $AD $04 $B7 $04 $C2 $04 $CD $04 $D9 $04 $E6 $04
.db $F4 $04 $03 $05 $12 $05 $22 $05 $34 $05 $46 $05
.db $AD $06 $B7 $06 $C2 $06 $CD $06 $D9 $06 $E6 $06
.db $F4 $06 $03 $07 $12 $07 $22 $07 $34 $07 $46 $07
.db $AD $08 $B7 $08 $C2 $08 $CD $08 $D9 $08 $E6 $08
.db $F4 $08 $03 $09 $12 $09 $22 $09 $34 $09 $46 $09
.db $AD $0A $B7 $0A $C2 $0A $CD $0A $D9 $0A $E6 $0A
.db $F4 $0A $03 $0B $12 $0B $22 $0B $34 $0B $46 $0B
.db $AD $0C $B7 $0C $C2 $0C $CD $0C $D9 $0C $E6 $0C
.db $F4 $0C $03 $0D $12 $0D $22 $0D $34 $0D $46 $0D
.db $AD $0E $B7 $0E $C2 $0E $CD $0E $D9 $0E $E6 $0E
.db $F4 $0E $03 $0F $12 $0F $22 $0F $34 $0F $46 $0F

MBMDrumReg:
.db $36 $37 $38 $16 $26 $17 $27 $18 $28

MBMModVal:
.dw  1,   2,   2,  -2,  -2,  -1,  -2,  -2,   2,   2

.ends

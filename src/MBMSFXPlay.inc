;=================================================================================
; RAM
;=================================================================================

.ramsection "FM SFX status" slot 3

MBMSFXStatus    db                  ; status: 0 = not playing, 1 = busy playing

.ends

.ramsection "FM SFX variables" slot 3

MBMSFXStart        dw               ; ROM address of SFX
MBMSFXPointer      dw               ; current address
MBMSFXLoopPoint    dw               ; the pointer to the loop begin
MBMSFXBank         db               ; bank where sfx is located
MBMSFXSkipFrames   db               ; wait counter for empty frames
MBMSFXLoopFlag     db               ; looping? 1= yes, 0 = no
MBMSFXDelayIVR     db               ; delay until instrument and volume are restored

.ends


;=================================================================================
; SFX extension for MoonBlaster 1.4 driver
; by Kagesan
;=================================================================================

.section "MBM SFX Start" free

MBMSFXPlayLoop:
    
    ld a, 1                         ; SFX _IS_ a looping one
    jp +

MBMSFXPlay:

    xor a                           ; SFX is _NOT_ a looping one
+:  ld (MBMSFXLoopFlag),a

    call  MBMSFXSilence             ; stop any sfx that might be playing
    
    inc a
    ld (MBMSFXStatus), a            ; set status to "playing"
    ld (MBMSFXSkipFrames), a        ; next frame follows immediately

    ld hl, (MBMSFXStart)            ; set pointer to start of sfx data
    ld (MBMSFXPointer), hl
    ld (MBMSFXLoopPoint), hl        ; loop pointer also points at the start for now
    
    xor a
    ld (MBMSFXDelayIVR), a
    
    ret

.ends


.section "MBM SFX Stop" free

MBMSFXStop:

    ld a, 22                        ; set number of frames until I/V restore
    ld (MBMSFXDelayIVR), a

    jp MBMSFXSilence                ; zero frequency

MBMSFXRestoreInstrVol:

    xor a
    ld (MBMSFXStatus), a            ; reset status to "not playing"
    ld (MBMSFXDelayIVR), a          ; reset delay timer

    ld a, (MBMChannelBuffer+6)      ; get instrument number
    dec a
    ld b, 0
    add a, a
    ld c, a
    ld hl,(MBMIVListAddr)

    
    add hl, bc
    ld a, (hl)                      ; get instrument from list
    cp 16
    jr c, +
    xor a

+:  rlca                            ; move instrument into upper nibble
    rlca
    rlca
    rlca
    ld b, a

    ld a, (MBMChannelBuffer+13)     ; get volume
    srl a
    srl a
    
    xor b                           ; combine volume with instrument
    
    ld c, $30
    jp FMOutFast                    ; restore instrument and volume, return

MBMSFXSilence:

    xor a
    ld c, $10                       ; set frequency to zero, key off
;    call FMOut
    call FMOutFast ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*
    nop   ;*    
    nop   ;*
    ld c, $20
;    jp FMOut
    jp FMOutFast ;*

.ends


.section "MBM SFX cancel loop" free

MBMSFXCancelLoop:

    xor a
    ld (MBMSFXLoopFlag), a
    
    ret

.ends


.section "MBM SFX Frame" free

MBMSFXFrame:

    ld a, (MBMSFXStatus)            ; check if sfx is playing
    or a                            ; no?
    ret z                           ; then just return
    
    ld a, (MBMSFXDelayIVR)          ; check if inside delay period
    cp 1
    jp c, +                         ; if not, proceed with sfx
    jp z, MBMSFXRestoreInstrVol     ; if end of delay, restore instrument/volume, end sfx
    dec a                           ; else reduce timer
    ld (MBMSFXDelayIVR), a          ; store new value
    
    ret

+:  ld a, (MBMSFXSkipFrames)        ; check if there are empty frames to skip
    dec a
    ld (MBMSFXSkipFrames), a
    ret nz

    ld a, (MBMSFXBank)              ; switch to bank containing the current sfx
    ld (BankSwitch), a
    
    ld hl, (MBMSFXPointer)
    
-:  ld a, (hl)                      ; read command / register
    cp 16
    jp c, _EndFrame
    cp 255                          ; check if loop point
    jp nz, +
    
    inc hl
    ld (MBMSFXLoopPoint), hl        ; set loop point
    
    jp -

+:  ld c, a                         ; if a > 15, then it's a register number
    inc hl                          ; move to data byte
    ld a, (hl)                      ; read data byte
    call FMOutFast
    inc hl                          ; move to next command / register byte

    jp -                            ; repeat until frame ends

_EndFrame:

    or a                            ; check if end of sfx
    jp z, _EndSFX

    ld (MBMSFXSkipFrames), a        ; store wait frames

    inc hl
    ld (MBMSFXPointer), hl

    ret

_EndSFX:

    ld a, (MBMSFXLoopFlag)          ; check if sfx is looping
    or a                            ; no?
    jp z, MBMSFXStop                ; then stop sfx, restore instrument and volume
    
    ld a, 1
    ld (MBMSFXSkipFrames), a        ; next frame follows immediately

    ld hl, (MBMSFXLoopPoint)        ; set pointer back to loop point
    ld (MBMSFXPointer), hl

    ret

.ends

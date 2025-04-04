INCLUDE "src/hardware.inc"

SECTION "Variables", WRAM0
  ; Variables go here

  BALL_X: db
  BALL_Y: db
  BALL_DX: db
  BALL_DY: db

  BAT_X: db
  BAT_Y: db
  BAT_DX: db

  FRAME: db
  SCORE10: db
  SCORE1: db


SECTION "RST_38",ROM0[$0038]
  RET

SECTION "Header", ROM0[$100]

	jp EntryPoint

	ds $150 - @, 0 ; Make room for the header



EntryPoint:
	; Shut down audio circuitry
	ld a, 0
	ld [rNR52], a

  di

	; Do not turn the LCD off outside of VBlank
WaitVBlank:
	; ldh a, [rLY]
	; cp 144
	; jp c, WaitVBlank

	; Turn the LCD off
	ld a, 0
	ld [rLCDC], a

	; Copy the tile data
	ld de, Tiles
	ld hl, $9000
	ld bc, TilesEnd - Tiles
CopyTiles:
	ld a, [de]
	ld [hli], a
	inc de
	dec bc
	ld a, b
	or a, c
	jp nz, CopyTiles

	; Copy the tilemap
	; ld de, Tilemap
	; ld hl, $9800
	; ld bc, TilemapEnd - Tilemap
; CopyTilemap:
; 	ld a, [de]
; 	ld [hli], a
; 	inc de
; 	dec bc
; 	ld a, b
; 	or a, c
; 	jp nz, CopyTilemap

	; Turn the LCD on
	ld a, LCDCF_ON | LCDCF_BGON
	ld [rLCDC], a

	; During the first (blank) frame, initialize display registers
	ld a, %11100100
	ld [rBGP], a

Main:
  call ClearScreen

  ; initialize ball x, y
  ld a, 10
  ld [BALL_X], a
  ld a, 5
  ld [BALL_Y], a

  ; initialize bat x,y
  ld a, 10
  ld [BAT_X], a
  ld a, 15
  ld [BAT_Y], a

  ; initialize ball dx, dy
  ld a, 1
  ld [BALL_DX], a
  ld a, 1
  ld [BALL_DY], a

  ; Initialize global variables
  ld a, 0
  ld [wCurKeys], a
  ld [wNewKeys], a

  ;initalize frame counter for ball
  ld a, 6
  ld [FRAME], a


.Mainloop:
  ; wait for vblank
  ld a, 1
  call VBlankWaitA

  ; plot score10
  ld a, [SCORE10]
  add a, 3
  ld [$9800], a
  ; plot score1
  ld a, [SCORE1]
  add a, 3
  ld [$9801], a

  ; clear the bat
  ld a, 0
  call PlotBat

  ; get input
  call UpdateKeys
  ; move bat based on input
  call MoveBat

  ; plot the bat
  ld a, 2
  call PlotBat

  ; plot ball every nth frame
  ld a, [FRAME]
  dec a
  ld [FRAME], a
  jr nz, .noballmove
  ld a,60
  ld [FRAME], a

  ; clear ball
  ld a, 0
  call PlotBall
  ; move ball n stuff
  call MoveBall
  ; plot ball
  ld a, 1
  call PlotBall

.noballmove:
  jp .Mainloop

  jp Done

MoveBall:
  ; move the ball and check for collision
  ; check if bat and ball collide
  ld a, [BALL_X]
  ld b,a
  ld a, [BAT_X]
  cp a,b
  jr nz, .nocollide
  ld a, [BALL_Y]
  ld b,a
  ld a, [BAT_Y]
  dec a ; check when the bat is 1 pixel above the ball
  cp a,b
  jr nz, .nocollide
  ld a, [BALL_DY]
  cpl a
  inc a
  ld [BALL_DY], a

  ; ball and bat have collided
  ; increment score
  ld a, [SCORE1]
  inc a
  cp a, 10
  ld [SCORE1], a
  jr nz, .incdone
  ; increment 10 digit
  ld a, [SCORE10]
  inc a
  ld [SCORE10], a
  ld a, 0
  ld [SCORE1], a
  .incdone:
  .nocollide:

  ; add ball dx
  ld a, [BALL_X]
  ld b, a
  ld a, [BALL_DX]
  add a, b

  bit 7,a
  jr nz, .xbouncel ; A >= 128
  cp a, 20
  jr c, .skipx ; A < 20

.xbouncer:
  ld a, [BALL_DX]
  cpl a
  inc a
  ld [BALL_DX], a
  ld a, 19
  jr .skipx

.xbouncel:
  ld a, [BALL_DX]
  cpl a
  inc a
  ld [BALL_DX], a
  ld a, 0

.skipx:
  ld [BALL_X], a

  ; add ball dy
  ld a, [BALL_Y]
  ld b, a
  ld a, [BALL_DY]
  add a, b

  bit 7,a
  jr nz, .ybouncet ; A >= 128
  cp a, 18
  jr c, .skipy ; A < 18

.ybounceb:
  ld a, [BALL_DY]
  cpl a
  inc a
  ld [BALL_DY], a

  ; clear score
  xor a
  ld [SCORE1], a
  ld [SCORE10], a

  ld a, 17
  jr .skipy

.ybouncet:
  ld a, [BALL_DY]
  cpl a
  inc a
  ld [BALL_DY], a
  ld a, 0

.skipy:
  ld [BALL_Y], a
  ret

MoveBat:
  ld a, [wNewKeys]
  bit 4, a ; check right
  jr nz, .move_right
  bit 5, a ; check left
  jr nz, .move_left
  jr .skip_move

  .move_right:
    ld a, [BAT_X]
    inc a
    ld [BAT_X], a
    jr .skip_move

  .move_left:
    ld a, [BAT_X]
    dec a
    ld [BAT_X], a

  .skip_move:
    ret

PlotBall:
  ; a is 0 to clear ball, 1 to plot ball
  push af

	ld a, [BALL_Y]
	ld b, a
	ld a, 32
	ld c, a
	call MultiplyBC   ; HL = Y * 32
	ld a, [BALL_X]
	ld e, a
	ld d, 0
	add hl, de        ; HL += X
	ld de, $9800
	add hl, de
  pop af
	ld [hl], a
	ret

PlotBat:
  ; a is 0 to clear ball, 2 to plot bat
  push af

  ld a, [BAT_Y]
  ld b, a
  ld a, 32
  ld c, a
  call MultiplyBC
  ld a, [BAT_X]
  ld e, a 
  ld d, 0
  add hl, de
  ld de, $9800
  add hl,de
  pop af
  ld [hl], a
  ret

; Multiply B * C, result in HL
MultiplyBC:
	ld hl, 0
.loop:
	ld a, b
	or a
	ret z

	ld e, c
	ld d, 0
	add hl, de

	dec b
	jr .loop

ClearScreen:
  ld a, 1
  call VBlankWaitA
  ld hl, $9800
  ld bc, 32*32
 .loop:
 xor a 
  ld [hli], a
  dec bc
  ld a, b
  or c
  jr nz, .loop
  ret

VBlankWaitA:
.wait:
  push af
  ; wait until rLY >= 144
.loop:
  ldh a, [rLY]
  cp 144
  jp c, .loop
  pop af

  dec a
  ret z

  push af
  ; wait until rLY < 144
.loop2:
  ldh a, [rLY]
  cp 144
  jr nc, .loop2
  pop af

  jr .wait

UpdateKeys:
  ; Poll half the controller
  ld a, P1F_GET_BTN
  call .onenibble
  ld b, a ; B7-4 = 1; B3-0 = unpressed buttons

  ; Poll the other half
  ld a, P1F_GET_DPAD
  call .onenibble
  swap a ; A7-4 = unpressed directions; A3-0 = 1
  xor a, b ; A = pressed buttons + directions
  ld b, a ; B = pressed buttons + directions

  ; And release the controller
  ld a, P1F_GET_NONE
  ldh [rP1], a

  ; Combine with previous wCurKeys to make wNewKeys
  ld a, [wCurKeys]
  xor a, b ; A = keys that changed state
  and a, b ; A = keys that changed to pressed
  ld [wNewKeys], a
  ld a, b
  ld [wCurKeys], a
  ret

.onenibble
  ldh [rP1], a ; switch the key matrix
  call .knownret ; burn 10 cycles calling a known ret
  ldh a, [rP1] ; ignore value while waiting for the key matrix to settle
  ldh a, [rP1]
  ldh a, [rP1] ; this read counts
  or a, $F0 ; A7-4 = 1; A3-0 = unpressed keys
.knownret
  ret

Done:
	jp Done


SECTION "Tile data", ROM0

Tiles:
  ;bg
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000

  ;ball
  db %00000000
  db %00000000
  db %00111100
  db %00111100
  db %01111110
  db %01111110
  db %01111110
  db %01111110
  db %01111110
  db %01111110
  db %01111110
  db %01111110
  db %00111100
  db %00111100
  db %00000000
  db %00000000

  ;bat
  db %11111111
  db %11111111
  db %11111111
  db %11111111
  db %11111111
  db %11111111
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000
  db %00000000

  ;0
  db %00111100
  db %00111100
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %00111100
  db %00111100

  ;1
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000
  db %00011000

  ;2
  db %00111000
  db %00111000
  db %01101100
  db %01101100
  db %01001100
  db %01001100
  db %00011000
  db %00011000
  db %00110000
  db %00110000
  db %01100000
  db %01100000
  db %01111110
  db %01111110
  db %00000000
  db %00000000

  ;3
  db %00111100
  db %00111100
  db %01000010
  db %01000010
  db %00000100
  db %00000100
  db %00011000
  db %00011000
  db %00000100
  db %00000100
  db %01000010
  db %01000010
  db %00111100
  db %00111100
  db %00000000
  db %00000000

  ;4
  db %00001000
  db %00001000
  db %00011000
  db %00011000
  db %00101000
  db %00101000
  db %01001000
  db %01001000
  db %01111110
  db %01111110
  db %00001000
  db %00001000
  db %00001000
  db %00001000
  db %00000000
  db %00000000

  ;5
  db %01111110
  db %01111110
  db %01000000
  db %01000000
  db %01111100
  db %01111100
  db %00000010
  db %00000010
  db %00000010
  db %00000010
  db %01000010
  db %01000010
  db %00111100
  db %00111100
  db %00000000
  db %00000000

  ;6
  db %00111100
  db %00111100
  db %01000000
  db %01000000
  db %01111100
  db %01111100
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %00111100
  db %00111100
  db %00000000
  db %00000000

  ;7
  db %01111110
  db %01111110
  db %00000010
  db %00000010
  db %00000100
  db %00000100
  db %00001000
  db %00001000
  db %00010000
  db %00010000
  db %00100000
  db %00100000
  db %00100000
  db %00100000
  db %00000000
  db %00000000

  ;8
  db %00111100
  db %00111100
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %00111100
  db %00111100
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %00111100
  db %00111100
  db %00000000
  db %00000000

  ;9
  db %00111100
  db %00111100
  db %01000010
  db %01000010
  db %01000010
  db %01000010
  db %00111110
  db %00111110
  db %00000010
  db %00000010
  db %00000010
  db %00000010
  db %00111100
  db %00111100
  db %00000000
  db %00000000

TilesEnd:

SECTION "Tilemap", ROM0

Tilemap:
TilemapEnd:

SECTION "Input Variables", WRAM0
wCurKeys: db
wNewKeys: db

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//  Copyright 2023, Jonathan Hart
//  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,
//  either version 3 of the License, or (at your option) any later version.
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE. See the GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


// going to hold off on this a bit.
#ifdef TACTYK_FHTEST__ENABLE

#include "fhtest.h"
#include <sys/time.h>
#include <sys/resource.h>

// omitted functions:
//  trans_init() - tactyk static memory allocation does the job adequately.

char *fh_tactyk_src = R"""(
    // fhourstones.tactykpl


    // field width aliases
    ;define byte i8
    ;define short i16
    ;define int i32
    ;define long i64

    // constants
    define WIDTH 7
    define HEIGHT 6
    define H1 7
    define H2 8
    define HEIGHT_x2 12
    define H1_x2 14
    define H2_x2 16
    define SIZE 42
    define SIZE1 49

    // 1<<H1 - 1
    define COL1 127
    // 1<<SIZE1 - 1
    define ALL1 562949953421311
    // ALL1 / COL1
    define BOTTOM 4432676798593
    // BOTTOM << HEIGHT
    define TOP 283691315109952

    // 49-26
    define SIZE1_minus_LOCKSIZE 23

    define mv0 {
        assign a 0
        fastcall MAKEMOVE
    }
    define mv1 {
        assign a 1
        fastcall MAKEMOVE
    }
    define mv2 {
        assign a 2
        fastcall MAKEMOVE
    }
    define mv3 {
        assign a 3
        fastcall MAKEMOVE
    }
    define mv4 {
        assign a 4
        fastcall MAKEMOVE
    }
    define mv5 {
        assign a 5
        fastcall MAKEMOVE
    }
    define mv6 {
        assign a 6
        fastcall MAKEMOVE
    }
    define back {
        fastcall BACKMOVE
    }

    // globals
    // heights is padded one byte for alignment
    // one word of padding is appended to the end
    //
    //  (because the last item is a 4-byte word and tactyk-vm forbids offsets referencing anything that starts within the last 7 bytes of a struct.
    //     Padding can be omitted if some other 8-byte word is moved to the end, but I'd rather leave it as-is to demonstrate the compromise).
    //
    //  struct definition:
    //      Structs define a memory layout.  This generates a set of compile-time symbols (binding names to memory offsets) to help make it a bit easier to
    //      store structured data in memory.
    //
    //      >       Return to byte offset zero and restart placing items from that position (retaining the previously defined symbols)
    //      .       Start the next 2+ items at the same byte offset (union type)
    //      <int>   Byte length of field.
    //      <name>  Name of field
    //
    /       The <name> is mandatory.  Everything else is optional.  Default field length is 8 bytes.
    struct state {
        > 8 color0
        8 color1
        > 16 color
        8 heights
        8 nplies
        64 moves
        4 htindex
        4 lock
        8 posed
        8 nodes
        8 msecs
        4 bookply
        4 reportply
        8 padding
    }

    mem state 1

    define HISTSIDE_SIZE 392
    struct history {
        HISTSIDE_SIZE items
    }
    mem history 2

    // a local namespace for function AB
    //  The main [and original] purpose the is the "av" byte array,
    //  But a severe need for extra storage space asose on account of hashindx, hashlock, and poscnt.
    //  So, this also serves as a primitive form of stack allocation.
    struct local {
        8 av
        4 hashindx
        4 hashlock
        8 poscnt
        8 ttscore
    }
    mem local 65536

    define LOCKSIZE 26
    define TRANSIZE 8306069
    define HT_SIZE 66448552
    define SYMMREC 10
    define UNKNOWN 0
    define LOSS 1
    define DRAWLOSS 2
    define DRAW 3
    define DRAWWIN 4
    define WIN 5
    define LOSSWIN 6

    define BOOKPLY 0
    define REPORTPLY 2

    mem ht HT_SIZE

    INIT:
        bind addr1 state
        bind addr2 ht
        bind addr3 history
        bind addr4 local
        fastreturn

    INITHISTORY:
        offset addr3 0
        fastcall INITHISTORY_SIDE
        offset addr3 392
        fastcall INITHISTORY_SIDE
        lwreturn

    // generate the initial state for the history table.
    //  In principle, I wouldn't mind storing it into a constant array in C, then using a c call to copy it over (especially as Fhourstones does not include it in its timekeeping),
    //  BUT, it did draw attention to defective min & max instructions, as well as an error in setting up memory from structs....
    INITHISTORY_SIDE:
        define a_i a
        define b_h b
        define c_index c

        assign a_i 0

        INITHISTORY_SIDE_OUTERLOOP:
        assign e 4
        if a_i >= e INITHISTORY_SIDE_EXIT
            assign b_h 0

            INITHISTORY_SIDE_INNERLOOP:
            assign f 3
            if b_h >= f INITHISTORY_SIDE_EXIT_INNERLOOP
                // d = 4+min(3,i)
                assign d 4
                assign e 3
                min e a_i
                add d e

                // d += max(-1, min(3,h)-max(0,3-i) )
                    // max(0,3-i)
                    assign f 3
                    sub f a_i
                    assign e 0
                    max e f

                    //min(3,h)-max(0,3-i)
                    assign f 3
                    min f b_h
                    sub f e

                    assign e -1
                    max e f
                add d e

                // d += min(3,min(i,h))
                    assign e 3
                    assign f a_i
                    min f b_h
                    min e f
                add d e

                // d += min(3,h)
                    assign e 3
                    min e b_h
                add d e

                // compute a set of 4 offsets into history table and store into each position.
                // this section could probably compute partial products and move them in and out of the stash
                //  instead of repeating the calculation -- but that would be optimizing an initializer.
                assign c_index H1
                mul c_index a_i
                add c_index b_h
                shift left c_index 3
                store qword addr3 c_index d

                assign c_index WIDTH
                dec c_index
                sub c_index a_i
                mul c_index H1
                add c_index HEIGHT
                dec c_index
                sub c_index b_h
                shift left c_index 3
                store qword addr3 c_index d

                assign c_index H1
                mul c_index a_i
                add c_index HEIGHT
                dec c_index
                sub c_index b_h
                shift left c_index 3
                store qword addr3 c_index d

                assign c_index WIDTH
                dec c_index
                sub c_index a_i
                mul c_index H1
                add c_index b_h
                shift left c_index 3
                store qword addr3 c_index d

                inc b_h
                goto INITHISTORY_SIDE_INNERLOOP
            INITHISTORY_SIDE_EXIT_INNERLOOP:
            inc a_i
            goto INITHISTORY_SIDE_OUTERLOOP
        INITHISTORY_SIDE_EXIT:
        fastreturn

    RESET:
        //  fastcall
        //  clobbers a,b,c,d,e
        assign a 0
        store qword addr1 state.color0 a
        store qword addr1 state.color1 a
        store qword addr1 state.heights a
        store qword addr1 state.nplies a
        define a_ctr a
        define c_href c
        define d_hgt d
        assign c state.heights
        assign d 0

        RESETLOOP:
            store byte addr1 c_href d_hgt
            add d_hgt H1
            inc a_ctr
            inc c_href
            if a_ctr < WIDTH RESETLOOP
        fastreturn
        undefine a_ctr
        undefine c_href
        undefine d_hgt

    POSITIONCODE:
        //  fastcall
        //  clobbers a,b,c,d,e

        define a_np a
        define b_c0 b
        define c_c1 c

        load qword a_np addr1 state.nplies
        load qword b_c0 addr1 state.color0
        load qword c_c1 addr1 state.color1
        assign d BOTTOM
        ;break

        // take lsb of nplies
        //  if lsb is 0, add color0
        //  if lsb is 1, add color1
        and a_np 1
        if a_np == 0 PC_get_color0
        PC_get_color1:
            assign a c_c1
            goto PC_continue
        PC_get_color0:
            assign a b_c0
        PC_continue:

        // sum all the intermediate products and return
        add a b
        add a c
        add a BOTTOM
        fastreturn

        undefine a_np
        undefine b_c0
        undefine c_c1

    ISLEGAL:
        //  fastcall
        //  clobbers a,b

        // tactyk-pl is "misconfigured" for this -- move input newboard out of the way
        // so that 64-bit value TOP can be injected with transfer
        // then bitwise and and proceed.
        //copy b a

        assign b TOP
        and a b


        if a != 0 ISLEGAL_REJECT
        ISLEGAL_ACCEPT:
            assign a 1
            fastreturn
        ISLEGAL_REJECT:
            assign a 0
            fastreturn

    ISPLAYABLE:
        //  lwcall
        //  clobbers a,b,c,d,e,f

        assign f a


        define a_np a
        define b_clr b
        define c_aclr c
        load qword a_np addr1 state.nplies
        load qword b_clr addr1 state.color0
        load qword c_aclr addr1 state.color1
            // if lsb of nplies is 0, use color0
            // if lsb of nplies is 1, use color1
            and a_np 1
            if a_np == 0 ISPL_SKIPIT
            assign b_clr c_aclr

        ISPL_SKIPIT:
        // release a,c
        // capture b,d,f
        // repurpose e,
        define b_clr b
        define e_val e
        define f_col f
        define d_hght d

        assign d state.heights
        assign e_val 1

        add d_hght f_col
        load byte d_hght addr1 d_hght
        shift left e_val d_hght

        // combine intermediates, then transfer to a for a call
        or e_val b_clr
        assign a e_val

        // release b,c,d,e,f
        fastcall ISLEGAL
        lwreturn

        undefine a_np a
        undefine b_clr b
        undefine c_aclr c
        undefine e_val e
        undefine f_col f
        undefine d_hght d

    HASWON:
        //  fastcall
        //  clobbers a,b,c,d

        define a_input a
        define b_product b
        // intermediates c,d

        assign c a_input
        shift right c HEIGHT
        and c a_input
        assign d c
        shift right d HEIGHT_x2
        and c d

        assign b_product c

        assign c a_input
        shift right c H1
        and c a_input
        assign d c
        shift right d H1_x2
        and c d

        or b_product c

        assign c a_input
        shift right c H2
        and c a_input
        assign d c
        shift right d H2_x2
        and c d

        or b_product c

        assign c a_input
        shift right c 1
        and c a_input
        assign d c
        shift right d 2
        and c d

        or b_product c
        assign a b_product

        fastreturn

        undefine a_input
        undefine b_product

    ISLEGALHASWON:
        //  lwcall
        //  clobbers a,b,c,d
        assign d a
        fastcall ISLEGAL
        if a == 0 ISLEGALHASWON_NO
        assign a d
        fastcall HASWON
        if a == 0 ISLEGALHASWON_NO
        assign a 1
        lwreturn

        ISLEGALHASWON_NO:
            assign a 0
            lwreturn
    BACKMOVE:
        //  fastcall
        //  clobbers a,b,c,d,e

        define a_np a
        define b_move b
        define c_cref c
        define d_href d
        define e_hght e

        load qword a_np addr1 state.nplies

        // decrement nplies
        dec a_np
        store byte addr1 state.nplies a_np

        // get move for ply
        assign b state.moves
        add b_move a_np
        load byte b_move addr1 b_move

        // get and decrement height for move
        assign d_href state.heights
        add d_href b_move
        load byte e_hght addr1 d_href
        dec e_hght
        store byte addr1 d_href e_hght

        // release b,c,e
        undefine b_move
        undefine d_href

        define b_bit b

        // compute 1<<height
        //  ... but first take advantage of the 1 to extract lsb of nplies
        assign b_bit 1
        and a_np b_bit
        shift left b_bit e_hght

        undefine e_hght
        define e_color e


        // get color for ply (based on parity)
        assign c_cref state.color
        if a_np == 0 BM_OFS_SKIP
        BM_OFS_COLOR:
            add c_cref 8
        BM_OFS_SKIP:
        load qword e_color addr1 c_cref

        // flip a color bit (1<<height)
        xor e_color b_bit
        store qword addr1 c_cref e_color
        fastreturn

        undefine a_np
        undefine c_cref
        undefine e_color
        undefine b_bit

    MAKEMOVE:
        //  fastcall
        //  clobbers a,b,c,d,e,f


        define a_input a
        define b_np b
        define d_move d
        define e_cref e
        define f_href f

        load qword b_np addr1 state.nplies

        // first update moves and nplies
        //  (easy way to free up a couple registers without extra mem access)
        assign d_move state.moves
        add d_move b_np
        store byte addr1 d_move a_input
        inc b_np
        store byte addr1 state.nplies b_np

        // release c,d
        undefine d_move

        define c_bit c
        define d_color d

        // color offset for ply
        // inverted to avoid decrementing _np
        assign c_bit 1
        and b_np c_bit
        assign e_cref state.color
        if b_np != 0 MM_OFS_SKIP
        BM_OFS_COLOR:
            add e_cref 8
        MM_OFS_SKIP:
        load qword d_color addr1 e_cref
        assign f state.heights
        add f_href a_input

        // release a,b
        undefine a_input
        undefine b_np

        define b_hght b

        load byte b_hght addr1 f_href
        shift left c_bit b_hght
        inc b_hght
        store byte addr1 f_href b_hght

        // release b
        undefine b_hght

        xor d_color c_bit
        //break
        store qword addr1 e_cref d_color

        fastreturn

        undefine e_cref
        undefine f_href
        undefine c_bit
        undefine d_color

    EMPTYTT:
        // fastcall a,b,c

        assign a 0
        assign b 0

        store qword addr1 state.posed a
        ETT_LOOP:
            //store qword addr2 b a
            offset addr2 b
            store qword addr2 0 a
            add b 8
            if b < HT_SIZE ETT_LOOP
        fastreturn
    HASH:
        //  lwcall
        //  clobbers a,b,c,d,e
        define a_htemp a
        define b_htmp b
        define c_htemp2 c
        define d_nplies d

        fastcall POSITIONCODE
        load qword d_nplies addr1 state.nplies
        if d_nplies >= SYMMREC HASH_DONE

        HASH_TRY_SYM:
            undefine d_nplies
            define e_COL1 e

            assign c_htemp2 0
            assign e_COL1 COL1

            assign b_htmp a_htemp
            HASH_TRY_SYM_LOOP:
                if b_htmp == 0 HASH_TRY_SYM_2
                shift left c_htemp2 H1
                assign d b_htmp
                and d e_COL1
                or c_htemp2 d
                shift right b_htmp H1
                goto HASH_TRY_SYM_LOOP
            HASH_TRY_SYM_2:
            if c_htemp2 >= a_htemp HASH_DONE
            assign a_htemp c_htemp2

        HASH_DONE:
        undefine b_htmp
        undefine c_htemp2
        undefine d_nplies
        undefine e_COL1

        define d_hti d

        assign d_hti a_htemp
        modulo d_hti TRANSIZE
        store dword addr1 state.htindex d_hti

        define b_lock b

        assign b_lock a_htemp
        shift right b_lock SIZE1_minus_LOCKSIZE
        store dword addr1 state.lock b_lock

        ;ccall print_hash_results
        lwreturn

        undefine d_hti
        undefine b_lock
    TRANSPOSE:
        //  lwcall
        //  clobbers a,b,c,d,e

        // I haven't yet decided to include operations of format "and <reg> 2^n-1"
        //  (or more broadly a bitwise subsetter) in ASM, but if going serious, these are a good candidates for superinstructions
        // leftshift + rightshift does accomplish bitwise subsetting, and superinstructions are available for both for shift amounts 1-63.

        lwcall HASH

        define a_hti a
        define b_he b
        define c_lock c
        define d_val d

        load dword a_hti addr1 state.htindex
        shift left a_hti 3
        offset addr2 a_hti
        load qword b_he addr2 0

        load dword c_lock addr1 state.lock

        // extract he.biglock (bits 38-63)
        assign d_val b_he
        shift right d_val 38

        if d_val != c_lock TRANSPOSE_2
            // extract he.bigscore (bits 0-2)
            assign a b_he
            and a 7
            lwreturn
        TRANSPOSE_2:
        // extract he.bigscore (bits 6-31)
        assign d_val b_he
        shift left d_val 32
        shift right d_val 38
        if d_val != c_lock TRANSPOSE_3
            // extract he.newscore (bits 3-5)
            assign a b_he
            shift right a 3
            and a 7
            lwreturn
        TRANSPOSE_3:
        assign a 0
        lwreturn

        undefine a_hti
        undefine b_he
        undefine c_lock
        undefine d_val
    TRANSTORE:
        //  fastcall

        //input args
        define a_x a
        define b_lock b
        define c_score c
        define d_work d

        load qword e addr1 state.posed
        inc e
        store qword addr1 state.posed e

        define e_he e

        shift left a_x 3
        offset addr2 a_x
        load qword e_he addr2 0

        undefine a_x

        //struct hashentry {
        //  unsigned biglock:26;
        //  unsigned bigwork:6;
        //  unsigned newlock:26;
        //  unsigned newscore:3;
        //  unsigned bigscore:3;
        //};


        define a_choice a

        // extract he.biglock (bits 38-63)
        assign a_choice e_he
        shift right a_choice 38
        if a_choice == b_lock TRANSTORE_BIG

        // extract he.bigwork (bits 32-37)
        assign a_choice e_he
        shift right a_choice 32
        and a_choice 63
        if d_work >= a_choice TRANSTORE_BIG

        // update e_he by shoving old values off the register (with bitshifts),
        // rotating/shifting it into position to combine with the replacement values (with bitwise or)
        // then rotate it b.ack into its original position
        //  JUST LIKE FREIGHT.

        TRANSTORE_NEW:
            rotate right e_he 3
            shift right e_he 29
            shift left e_he 26
            or e_he b_lock
            shift left e_he 3
            or e_he d_work
            rotate left e_he 3
            goto TRANSTORE_END
        TRANSTORE_BIG:
            shift left e_he 32
            shift right e_he 35
            shift left e_he 3
            or e_he c_score
            rotate right e_he 32
            or e_he d_work
            rotate right e_he 6
            or e_he b_lock
            rotate left e_he 38

        TRANSTORE_END:
        store qword addr2 0 e_he
        ;break
        fastreturn
        // shift left a_x 3
    SOLVE:
        //  lwcall

        assign f 0

        store qword addr1 state.nodes f
        store qword addr1 state.msecs f

        define b_otherside b
        define c_nplies c

        define e_i e
        define f_side f

        assign d state.color0

        load qword c_nplies addr1 state.nplies
        assign f_side c_nplies
        and f_side 1
        assign b_otherside f_side
        xor b_otherside 1
        stash 1 a
        shift left b_otherside 3
        add b_otherside state.color0
        load qword b_otherside addr1 b_otherside
        assign a b
        fastcall HASWON
        assign b 1
        if a == b SOLVE_EXIT
        assign e_i WIDTH

        SOLVE_WINCHK:
        dec e_i
        if e_i < 0 SOLVE_RUNIT
        assign b state.heights
        add b e_i
        load byte b addr1 b
        assign a 1
        shift left a b
        or a f_side
        lwcall ISLEGALHASWON
        if a == 0 SOLVE_WINCHK
        assign a WIN
        goto SOLVE_EXIT

        SOLVE_RUNIT:


        undefine b_otherside
        undefine c_nplies
        undefine e_i
        undefine f_side

        define c_nplies c

        load qword c_nplies addr1  state.nplies
        add c_nplies REPORTPLY
        store dword addr1 state.reportply c_nplies
        sub c_nplies REPORTPLY
        add c_nplies BOOKPLY
        store dword addr1 state.bookply c_nplies

        undefine c_nplies

        ccall millisecs a
        store qword addr1 state.msecs a

        assign a LOSS
        assign b WIN

        mctxcall AB

        load qword c addr1 state.msecs
        ccall millisecs b
        inc b
        sub b c
        store qword addr1 state.msecs b

        // return [WIN or LOSS or SCORE (from ab)]
        SOLVE_EXIT:
        lwreturn

    AB:
        // mctxcall 1.a

        // set alpha and beta inputs aside for a while
        swap a b
        stash a1b1
        // use av[ mctxstackpos to +32 ] as local storage space.
        mctxstacksgetpos a 1
        shift left a 5
        offset addr4 a

        // clear av[]
        assign e 0
        store qword addr4 0 e

        define b_nplies b
        define d_sideofs d
        define e_side e
        define e_other e

        load qword e_side addr1 state.nplies
        assign c 1
        and e_side c
        load qword c addr1 state.nodes
        inc c
        store qword addr1 state.nodes c

        // main offset into the history buffer (history[side])
        assign d_sideofs HISTSIDE_SIZE
        mul d_sideofs e_side
        offset addr3 d_sideofs

        stash @3@4

        load qword b_nplies addr1 state.nplies
        assign e_other b_nplies

        inc b_nplies
        sub b_nplies SIZE
        if b_nplies == 0 AB_DRAW

        and e_other 1
        xor e_other 1
        shift left e_other 3
        // add e_other state.color0
        load qword e_other addr1 e_other

        undefine b_nplies
        undefine d_sideofs
        undefine e_side

        define a_height a
        define b_nav b
        define c_i c
        define f_bit f
        define f_winontop f

        assign b_nav 0
        assign c_i 0

        AB_LOOP1:

        if c_i >= WIDTH AB_EXIT_LOOP1
        assign a_height state.heights
        add a_height c_i
        load byte a_height addr1 a_height
        assign f_bit 1
        shift left f_bit a_height

        // ISLEGALHASWON coming up, so 4 registers need to be stashed.
        //  stash early to make the most of it.
        stash a2b2c2d2

        define a_newbrd a

        //check opponent move
        assign a_newbrd f_bit
        or a_newbrd e_other
        fastcall ISLEGAL
        unstash b2
        if a == 0 AB_CONTINUE_LOOP1

        //winontop
        assign a f_bit
        shift left a 1
        or a e_other
        lwcall ISLEGALHASWON

        // reposition f_bit to rebuild a_newbrd in argument position
        // f now indicates "winontop"
        // f reverts to f_bit for AB_CONSEQUENCE_CHECK (but persistance then is irrelevant)
        swap a_newbrd f_winontop
        or a_newbrd e_other
        fastcall HASWON
        // is there an immediate threat?
        if a == 0 AB_NO_OPPONENT_WIN

        //is it a double loss?
        ;break
        if f_winontop != 0 AB_LOSS

        unstash a2b2c2d2
        assign b_nav 0
        store byte addr4 b_nav c_i
        inc b_nav
        stash b2

        // does the opponent get to win?
        AB_CONSEQUENCE_CHECK:
        inc c_i

        if c_i >= WIDTH AB_EXIT_LOOP1
        assign a_height state.heights
        add a_height c_i
        load byte a_height addr1 a_height
        assign f_bit 1
        shift left f_bit a_height
        assign a_newbrd e_other
        or a_newbrd f_bit
        assign f c_i
        lwcall ISLEGALHASWON
        ;break
        if a != 0 AB_LOSS
        unstash a2b2c2d2
        assign c_i f
        goto AB_CONSEQUENCE_CHECK


        AB_NO_OPPONENT_WIN:
        unstash a2b2c2d2
        if f_winontop != 0 AB_CONTINUE_LOOP1
        AB_NO_WINONTOP:
        store byte addr4 b_nav c_i
        inc b_nav
        AB_CONTINUE_LOOP1:
        inc c_i
        goto AB_LOOP1

        AB_EXIT_LOOP1:

        // {
        //    situation:
        //    retain nav
        //    hold alpha,beta
        //    AB recurses soon.
        //}

        // retain b_nav

        undefine a_newbrd
        undefine a_height
        undefine c_i
        undefine f_bit
        undefine f_winontop

        ;break
        if b_nav == 0 AB_LOSS

        // if nplies == size-2, draw
        define c_nplies c
        load qword c_nplies addr1 state.nplies
        add c_nplies 2
        sub c_nplies SIZE

        ;break
        if c_nplies == 0 AB_DRAW
        undefine c_nplies

        // if b_nav == 1, recurse once and return the result
        if b_nav != 1 AB_DO_TRANSPOSE
        ;assign a 0
        load byte a addr4 0
        fastcall MAKEMOVE

        unstash a1b1

        ;break

        negate a
        negate b
        add a LOSSWIN
        add b LOSSWIN
        mctxcall AB
        ;break
        negate a
        add a LOSSWIN
        assign f a
        fastcall BACKMOVE
        assign a f

        ;break

        mctxreturn

        AB_DO_TRANSPOSE:

        assign f b_nav
        undefine b_nav

        define a_beta a
        define b_alpha b
        define d_ttscore d
        lwcall TRANSPOSE

        assign d a
        unstash a1b1
        ;break

        if d_ttscore == UNKNOWN AB_DO_SEARCH
        if d_ttscore == DRAWLOSS AB_TTS_DRAWLOSS
        if d_ttscore == DRAWWIN AB_TTS_DRAWWIN

        goto AB_RET_TTSCORE

        AB_TTS_DRAWLOSS:
            assign a_beta DRAW
            if d_ttscore <= b_alpha AB_RET_TTSCORE
            goto AB_DO_SEARCH

        AB_TTS_DRAWWIN:
            assign b_alpha DRAW
            if b_alpha >= a_beta AB_RET_TTSCORE
            goto AB_DO_SEARCH

        AB_RET_TTSCORE:
            assign a d_ttscore
            ;break
            mctxreturn

        undefine a_beta
        undefine b_alpha


        AB_DO_SEARCH:
        // persistant-across-recursion vars
        //  These need to be unstashed prior to recursion (the stash is global, and there is not yet a formal definition for arbitrary-sized local data storage)
        //      (and even when such a definition exists, it will probably still be worthwhile to pack all locals into just the registers for microcontext calls)
        define a_beta a
        define b_alpha b
        define c_score c

        define e_i e
        define f_nav f

        // used on both sides (but for different purposes)
        define d_val d

        assign c_score LOSS

        stash a1b1c1d1e1

        load dword a addr1 state.htindex
        store dword addr4 local.hashindx a

        load dword a addr1 state.lock
        store dword addr4 local.hashlock a

        load qword a addr1 state.posed
        store qword addr4 local.poscnt a

        store qword addr4 local.ttscore d_ttscore

        // transient vars
        define a_j a
        define b_v b
        define c_l c

        assign e_i 0
        AB_LOOP2:
        if e_i >= f_nav AB_EXIT_LOOP2
        // val = history[side][(int)height[av[l = i]]];
        load byte d_val addr4 e_i
        add d_val state.heights
        load byte d_val addr1 d_val
        shift left d_val 3
        load qword d_val addr3 d_val

        assign c_l e_i
        assign a_j e_i
        inc a_j

        AB_LOOP2_INNER1:
        if a_j >= f_nav AB_EXIT_LOOP2_INNER1

        // v = history[side][(int)height[av[j]]];
        load byte b_v addr4 a_j
        add b_v state.heights
        load byte b_v addr1 b_v
        shift left b_v 3
        load qword b_v addr3 b_v

        if b_v <= d_val AB_CONTINUE_LOOP2_INNER1
        assign d_val b_v
        assign c_l a_j


        AB_CONTINUE_LOOP2_INNER1:
        inc a_j
        goto AB_LOOP2_INNER1

        AB_EXIT_LOOP2_INNER1:
        load byte a_j addr4 c_l

        undefine b_v

        AB_LOOP2_INNER2:
        if c_l <= e_i AB_EXIT_LOOP2_INNER2
        dec c_l
        load byte b addr4 c_l
        inc c_l
        store byte addr4 c_l b
        dec c_l
        goto AB_LOOP2_INNER2

        AB_EXIT_LOOP2_INNER2:
        store byte addr4 e_i a_j
        stash d1e1f1

        ;break
        fastcall MAKEMOVE

        unstash a1b1c1d1e1f1

        ;break

        negate a
        negate b
        add a LOSSWIN
        add b LOSSWIN

        mctxcall AB
        ;break

        assign d a

        negate d
        add d LOSSWIN

        ;break

        stash d1

        fastcall BACKMOVE

        unstash a1b1c1d1e1f1@3@4
        ;break

        undefine a_l
        undefine c_j

        define a_beta a
        define b_alpha b
        define c_score c
        ; d_val
        define e_i e
        define f_nav f

        ;break
        if d_val <= c_score AB_CONTINUE_LOOP2
        assign c_score d_val
        stash c1

        ;break
        if c_score <= b_alpha AB_CONTINUE_LOOP2

        undefine d_val
        define d_nplies d
        load qword d_nplies addr1 state.nplies

        ;stash f1

        load dword f addr1 state.bookply

        ;break
        if d_nplies < f AB_CONTINUE_LOOP2

        assign b_alpha c_score
        stash b1

        if b_alpha < a_beta AB_CONTINUE_LOOP2

        ;break

        undefine a_beta
        undefine b_alpha
        undefine d_nplies

        ;break
        if c_score != DRAW AB_END_CHK2
        unstash f1
        dec f_nav

        ;break
        if e_i >= f_nav AB_END_CHK2
        assign c_score DRAWWIN
        stash c1

        undefine f_nav

        AB_END_CHK2:
        ;break
        if e_i <= 0 AB_EXIT_LOOP2

        define a_i a

        assign a_i 0

        ;break

        AB_LOOP2_INNER3:
        ;break
        if a_i >= e_i AB_EXIT_LOOP2_INNER3
        load byte b addr4 a_i
        add b state.heights
        load byte b addr1 b
        mul b 8
        load qword f addr3 b
        dec f
        store qword addr3 b f
        inc a_i
        ;break
        goto AB_LOOP2_INNER3

        AB_EXIT_LOOP2_INNER3:

        load byte b addr4 e_i
        add b state.heights
        load byte b addr1 b
        mul b 8
        load qword f addr3 b
        add f e_i
        store qword addr3 b f
        ;break

        goto AB_EXIT_LOOP2

        AB_CONTINUE_LOOP2:
        unstash f1
        inc e_i
        ;break
        goto AB_LOOP2

        AB_EXIT_LOOP2:


        undefine e_i

        define b_posed b
        define c_score c
        define d_ttscore d

        ;assign d local.ttscore
        load qword d_ttscore addr4 local.ttscore
        negate d_ttscore
        add d_ttscore LOSSWIN
        if c_score != d_ttscore AB_NOT_A_DRAW
        assign c_score DRAW

        AB_NOT_A_DRAW:

        undefine d_ttscore
        define d_work d

        load qword e addr4 local.poscnt
        load qword b_posed addr1 state.posed
        negate e
        add e b_posed
        assign d_work 0

        AB_WORK_LOOP:
        shift right e 1
        if e == 0 AB_EXIT_WORK_LOOP
        inc d_work
        goto AB_WORK_LOOP

        AB_EXIT_WORK_LOOP:

        store qword addr4 local.poscnt e

        undefine b_posed

        define a_hashindx a
        define b_hashlock b

        //assign a_hashindx local.hashindx
        //assign b_hashlock local.hashlock

        load dword a_hashindx addr4 local.hashindx
        load dword b_hashlock addr4 local.hashlock


        stash c3d3
        ;break
        // a=hashindx, b=hashlock, c=score, d=work
        fastcall TRANSTORE
        ;break
        unstash c3d3

        load qword e addr1 state.nplies
        load dword f addr1 state.reportply

        assign a c_score

        if e > f AB_DONE
        assign b d_work
        ccall printreport

        AB_DONE:
        ;break
        mctxreturn

        AB_DRAW:
            assign a DRAW
            ;break
            mctxreturn
        AB_LOSS:
            assign a LOSS
            ;break
            mctxreturn


    PREPTEST:
        fastcall INIT
        lwcall INITHISTORY
        fastcall RESET

        lwreturn
    DOTEST:
        load qword a addr1 state.nplies

        //printf("\nSolving %d-ply position after ", nplies);
        ccall introprint

        //printMoves();
        ccall printmoves

        //puts(" . . .");
        ccall print3dots

        fastcall EMPTYTT
        lwcall SOLVE

        //poscnt = posed;
        load qword c addr1 state.posed

        //for (work=0; (poscnt>>=1) != 0; work++) ; //work = log of #positions stored
        assign b 0
        DOTEST_WORK_LOOP:
        shift right c 1
        if c == 0 DOTEST_EXIT_WORK_LOOP
        inc b
        goto DOTEST_WORK_LOOP
        DOTEST_EXIT_WORK_LOOP:

        //printf("score = %d (%c)  work = %d\n", result, "#-<=>+"[result], work);
        ccall printwork

        //assign a state.nodes
        //assign c state.msecs

        //printf("%lu pos / %lu msec = %.1f Kpos/sec\n", nodes, msecs, (double)nodes/msecs);
        load qword a addr1 state.nodes
        load qword b addr1 state.msecs
        ccall printthetime

        //htstat();

        lwreturn

    TEST1:
        lwcall PREPTEST
        // 45461667
        mv3 mv4 mv3 mv5 mv0 mv5 mv5 mv6
        lwcall DOTEST
        exit
    TEST2:
        lwcall PREPTEST
        // 35333571
        mv3 mv4 mv2 mv2 mv2 mv4 mv6 mv0
        lwcall DOTEST
        exit
    TEST3:
        lwcall PREPTEST
        // 13333111
        mv0 mv2 mv2 mv2 mv2 mv0 mv0 mv0
        lwcall DOTEST
        exit
    TEST4:
        lwcall PREPTEST
        lwcall DOTEST
        exit

    MAIN:
        fastcall INIT
        lwcall INITHISTORY
        fastcall RESET

        mv3 mv4 mv3 mv5 mv0 mv5 mv5 mv6

        exit

        fastcall POSITIONCODE
        ccall print_val
        lwcall HASH
        assign a 0
        load qword a addr1 a
        fastcall HASWON
        ccall print_val
        assign a 8
        load qword a addr1 a
        fastcall HASWON
        ccall print_val

        //ccall printmoves
        exit
    TLWC:
        break
        break
        mctxreturn

    DBG_LWC:
        mctxcall TLWC
        exit

    DOSTUFF:
        ;fastcall DOSTUFFC
        assign a 376345
        break
        assign b 200000
        break
        mctxcall DOSTUFFB
        modulo a b
        exit

    DOSTUFFB:
        assign a 1234
        assign b 4321
        assign c 88888
        mctxreturn

    DONE:
        exit

    DIAG:
        cpuclocks
        exit
)""";

// mv1 mv2 mv3 mv1 mv1 mv2 mv2 mv2 mv5 mv4
//  mv3 mv4 mv5 mv6 mv3 mv4 mv5 mv6 mv3 mv4 mv5 mv6 mv3 mv4 mv5 mv6 mv0 mv0

#define FH_SIZE 42
#define FH_WIDTH 7
/*
    struct state {
        > 8 color0
        8 color1
        > 16 color
        8 heights
        8 nplies
        64 moves
        4 htindex
        4 lock
        8 posed
        8 nodes
        8 msecs
        4 bookply
        4 reportply
        8 padding
    }



*/
struct fh_gamestate {
    int64_t color0;
    int64_t color1;
    int8_t height[8];
    int64_t nplies;
    int8_t moves[64];
    int32_t htindex;
    int32_t lock;
    int64_t posed;
    int64_t nodes;
    int64_t msecs;
    int32_t bookply;
    int32_t reportply;
    int64_t padding;
};
struct local_vars {
    uint64_t av;
    uint32_t hashindx;
    uint32_t hashlock;
    uint64_t poscnt;
    uint64_t ttscore;
};

struct hashentry {
  unsigned bigscore:3;
  unsigned newscore:3;
  unsigned newlock:26;
  unsigned bigwork:6;
  unsigned biglock:26;
};


struct tactyk_asmvm__Program *fhprogram;
struct fh_gamestate *state;
struct hashentry *ht;
uint64_t* history;
struct local_vars *locals;

void introprint(uint64_t nplies) {
    printf("\nSolving %lu-ply position after ", nplies);
}
void print3dots() {
    puts(" . . .");
}

void printwork(uint64_t result, uint64_t work) {
    printf("score = %lu (%c)  work = %lu\n", result, "#-<=>+"[result], work);
}

extern void printthetime(uint64_t nodes, uint64_t msecs, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    uint64_t ipart = nodes/msecs;
    uint64_t fpart = ((nodes%msecs)*10)/msecs;
    printf("%lu pos / %lu msec = %lu.%lu Kpos/sec\n", nodes, msecs, ipart,fpart);

    // ISSUE:  Calling printthetime() from tactyk_asmvm causes printf to segfault if attempting to print a floating point number.
    //  The reason is not known, but maybe the setup for calling into C functions is incorrect or incomplete, or maybe there is some compiler optimization interfering.
    //("%lu pos / %lu msec = %.1f Kpos/sec\n", nodes, msecs, (double)nodes/msecs);
    //printf("%lu pos / %lu msec = %f Kpos/sec\n", nodes, msecs, 0.56f);
}

uint64_t testtx(uint64_t val) {
    printf("testtx %lu -> %lu\n", val, val+1234);
    return val + 1234;
}

void printboard() {
    printf("  c0: %lu\n  c1: %lu\n", state->color0, state->color1);
    for (int64_t r = 5; r >= 0; r--) {
        printf("   ");
        for (int64_t c = 0; c <= 42; c+= 7) {
            int64_t pos = r+c;
            int64_t mask = 1l<<pos;
            if ((state->color0 & mask) != 0) {
                printf("0 ");
            }
            else if ((state->color1 & mask) != 0) {
                printf("1 ");
            }
            else {
                printf(". ");
            }
        }
        printf("\n");
    }
}

// technically this is part of Fhourstones and shoudl be implemented in TACTYK-PL, as it is called occasionally during the timed function.
//  But that would necessitate adding a text buffer and a print function, so I'd rather skip it for now.
void printmoves() {
    for (int64_t i = 0; i < state->nplies; i++) {
        printf("%d", state->moves[i]+1);
    }

}

void printreport(int64_t score, int64_t work) {
    printmoves();
    printf("%c%ld\n", "#-<=>+"[score], work);
}
/*
    struct state {
        . 8 color0
        . 8 color1
        16 color
        8 heights
        8 nplies
        SIZE moves
        8 padding
    }
    */
void printstate() {
    printf("BOARD:\n");
    printboard();
    printf("NPLIES  %ld\n", state->nplies);
    printf("HEIGHTS %d %d %d %d %d %d %d\n", state->height[0], state->height[1], state->height[2], state->height[3], state->height[4], state->height[5], state->height[6]);
    printf("MOVES   ");

    for (int i = 0; i < FH_SIZE; i++) {
        printf("%d", state->moves[i]);
    }
    printf("\n");

}

void print_val(uint64_t val) {
    printf("value = %lu\n", val);
}

void print_passfail(uint64_t val) {
    if (val) {
        printf("PASS\n");
    }
    else printf("FAIL\n");
}

// Because I am not interested in integrating the Fhourstones C code into this project,
//   this benchmark is to be timed like Fhourstones (rather than using rdtsc).
//  This is also an example of an API function which violates TACTYK-VM security guidelines by
//  giving scripts a high-precision timer (better start packining now!).
uint64_t millisecs() {
  struct rusage rusage;
  getrusage(RUSAGE_SELF,&rusage);
  return (rusage.ru_utime.tv_sec * 1000 + rusage.ru_utime.tv_usec / 1000);
  //return 12345;
}

void print_hash_results(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) {
    printf("hash-results lock=%lu htindex=%lu htemp=%lu\n", b, d, a);
}

void dbgprint(struct tactyk_asmvm__Context *ctx) {
    printf("\n\n");

    fhprogram->debug_func(ctx->bank_A.rIPTR, NULL);

    uint64_t stpos = ctx->mctxstack_position[0]>>8;
    printf("mctx stackpos=%lu\n", stpos);
    printf("mctx-ret-ptrs:  ");
    for (int64_t i = 0; i < 32; i++) {
        printf("%lu ", ctx->mctxstack[MICROCONTEXT_SIZE*i+6]);
    }
    printf("\nmctx ret-iptr=%lu\n\n", ctx->mctxstack[MICROCONTEXT_SIZE*(stpos-1)+6]);

    printf("mctx-diag:  ");
    for (int64_t i = 0; i < 4; i++) {
        printf("%lu ", ctx->diagnostic_data[i]);
    }

    printf("\n\n");

    tactyk_asmvm__print_context(ctx);
    printf("\nBOARD:\n");
    printboard();
    printf("  nplies: %lu\n\n", state->nplies);
    printf("heights\n  %d %d %d %d %d %d %d\n", state->height[0], state->height[1], state->height[2], state->height[3], state->height[4], state->height[5], state->height[6]);
    printf("moves\n  ");
    for (int i = 0; i < 42; i++) {
        printf("%d ", state->moves[i]);
    }
    printf("\n");
    printf("history-c0\n  ");
    for (int i = 0; i < 49; i++) {
        printf("%ld ", history[0+i]);
    }
    printf("\nhistory-c1\n  ");
    for (int i = 0; i < 49; i++) {
        printf("%ld ", history[49+i]);
    }
    printf("\n");
    printf("state\n  nodes: %lu\n  msecs: %lu\n  bookply: %d\n  reportply: %d\n", state->nodes, state->msecs, state->bookply, state->reportply);

    printf("  htindex: %d\n  lock: %d\n  posed: %lu\n", state->htindex, state->lock, state->posed);
    if (ht != NULL) {
        struct hashentry h = ht[state->htindex];
        printf("ht[%d]\n  biglock: %d\n  bigwork: %d\n  newlock: %d\n  newscore: %d\n  bigscore: %d\n", state->htindex, h.biglock, h.bigwork, h.newlock, h.newscore, h.bigscore);
        int64_t _h = ((int64_t*)ht)[state->htindex];
        printf("-ht = %lu\n", _h);
    }
    struct local_vars *loc = &locals[stpos];
    printf("locals\n  av: %lu\n  hashindx: %d\n  hashlock: %d\n  poscnt: %d\n  ttscore: %d\n", loc->av, loc->hashindx, loc->hashlock, loc->poscnt, loc->ttscore);
}

void run_fh_test(struct tactyk_emit__Context *emitctx, struct tactyk_asmvm__Context *ctx)  {

    tactyk_emit__add_apifunc(emitctx, "print_passfail", print_passfail);
    tactyk_emit__add_apifunc(emitctx, "print_val", print_val);
    tactyk_emit__add_apifunc(emitctx, "print_hash_results", print_hash_results);
    tactyk_emit__add_apifunc(emitctx, "printmoves", printmoves);
    tactyk_emit__add_apifunc(emitctx, "millisecs", millisecs);
    tactyk_emit__add_apifunc(emitctx, "printreport", printreport);
    tactyk_emit__add_apifunc(emitctx, "introprint", introprint);
    tactyk_emit__add_apifunc(emitctx, "print3dots", print3dots);
    tactyk_emit__add_apifunc(emitctx, "printwork", printwork);
    tactyk_emit__add_apifunc(emitctx, "printthetime", printthetime);

    tactyk_emit__add_apifunc(emitctx, "testtx", testtx);
    printf("....\n");
    fhprogram = tactyk_pl__load(emitctx, &fh_tactyk_src, 1, false);
    //program = tactyk_pl__load(&srcb, 1);
    printf("generated %lu instructions \n", fhprogram->length);
   // tactyk_asmvm__invoke_debug(ctx, program, "DOSTUFF", dbgprint);


    struct tactyk_asmvm__memblock_highlevel *mb2 = tactyk_table__get_strkey(fhprogram->symbols.memtbl, "ht");
    ht = (uint64_t*)mb2->data;
    struct tactyk_asmvm__memblock_highlevel *mbhist = tactyk_table__get_strkey(fhprogram->symbols.memtbl, "history");
    history = (uint64_t*)mbhist->data;
    printf("%lu %lu %lu \n", mb2->memblock_id, mb2->num_entries, mb2->definition->byte_stride);
    struct tactyk_asmvm__memblock_highlevel *mb = tactyk_table__get_strkey(fhprogram->symbols.memtbl, "state");
    state = (struct fh_gamestate*) mb->data;
    struct tactyk_asmvm__memblock_highlevel *mbloc = tactyk_table__get_strkey(fhprogram->symbols.memtbl, "local");
    locals = (struct local_vars*) mbloc->data;
    //tactyk_asmvm__invoke(ctx, program, "INIT");
    //tactyk_asmvm__invoke_debug(ctx, program, "MAIN");
    //tactyk_asmvm__invoke(ctx, program, "TEST1");
    //tactyk_asmvm__invoke_debug(ctx, program, "MAIN");
   //ctx->stepper = 1;
    tactyk_asmvm__invoke(ctx, fhprogram, "DIAG");
    uint64_t c1 = ctx->diagnostic_data[0];

    //tactyk_asmvm__invoke(ctx, fhprogram, "TEST1");
    tactyk_asmvm__invoke_debug(ctx, fhprogram, "TEST2", dbgprint);

    tactyk_asmvm__invoke(ctx, fhprogram, "DIAG");
    uint64_t c2 = ctx->diagnostic_data[0];

    printf("cycle count: %lu\n", c2-c1);
    //printstate();

    //dbgprint(ctx);
    //printf("cyc: %lu\n", ctx->diagnostic_data[0]);

/*
    struct tactyk_asmvm__memblock *hmb = tactyk_table__get_strkey(program->symbols.memtbl, "history");
    int64_t* hdata = (int64_t*)hmb->data;
    for (int side = 0; side <= 49; side += 49) {
        for (int h = 0; h < 7; h++) {
            for (int i = 0; i < 7; i++) {
                printf("%ld ", hdata[side+7*i+h]);
            }
            printf("\n");
        }
        printf("\n");
    }
    */
    //for (int i = 0; i < 128; i++) {
    //    printf("%d ", mb->data[i]) ;
    //}
    //printf("\n");

    //uint64_t* data = (uint64_t*) mb2->data;
    //printf("%lu\n", data[8306068]);
    //printf("%lu\n", data[8306069]);
    //tactyk_asmvm__print_context(ctx);
}

#endif

>>SOURCE FORMAT FREE
       IDENTIFICATION DIVISION.
       PROGRAM-ID. sim.
       AUTHOR. Gemini.
       DATE-WRITTEN. 2023.
       *> ----------------------------------------------------------------
       *> GShare Branch Predictor in GnuCOBOL
       *> Fixes: PIC Z(0) error, Output formatting, Robust parsing.
       *> Compile: cobc -x -free sim.cbl -o sim_c
       *> Usage: ./sim_c gshare <M> <N> <trace_file>
       *> ----------------------------------------------------------------

       ENVIRONMENT DIVISION.
       CONFIGURATION SECTION.
       REPOSITORY.
           FUNCTION ALL INTRINSIC.

       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT TRACE-FILE ASSIGN TO WS-FILENAME
               ORGANIZATION IS LINE SEQUENTIAL
               FILE STATUS IS WS-FILE-STATUS.

       DATA DIVISION.
       FILE SECTION.
       FD  TRACE-FILE.
       01  TRACE-RECORD PIC X(256).

       WORKING-STORAGE SECTION.
       *> Command Line Arguments
       01  WS-CMD-ARGS       PIC X(255).
       
       01  WS-TYPE           PIC X(10).
       01  WS-M-STR          PIC X(10).
       01  WS-N-STR          PIC X(10).
       01  WS-FILENAME       PIC X(100).
       
       *> Integers for parameters
       01  M                 PIC 9(9) USAGE COMP-5.
       01  N                 PIC 9(9) USAGE COMP-5.
       
       *> Display Variables (Z suppresses zeros, 9 forces digit)
       01  DISP-M            PIC Z(9).
       01  DISP-N            PIC Z(9).
       
       *> Predictor State
       01  GHR               PIC 9(18) USAGE COMP-5 VALUE 0.
       01  PHT-SIZE          PIC 9(18) USAGE COMP-5.
       01  PHT-PTR           USAGE POINTER.
       
       *> File Processing variables
       01  WS-FILE-STATUS    PIC XX.
       01  WS-EOF            PIC X VALUE 'N'.
           88 END-OF-FILE    VALUE 'Y'.
       
       01  WS-LINE           PIC X(256).
       01  WS-PC-HEX         PIC X(20).
       01  WS-OUTCOME-STR    PIC X(10).
       01  WS-OUTCOME-CHAR   PIC X.
       
       01  PC-VAL            PIC 9(18) USAGE COMP-5.
       01  ACTUAL-TAKEN      PIC 9 USAGE COMP-5.
       
       *> Indexing Logic Variables
       01  MASK-VAL          PIC 9(18) USAGE COMP-5.
       01  PC-INDEX          PIC 9(18) USAGE COMP-5.
       01  GHR-SHIFTED       PIC 9(18) USAGE COMP-5.
       01  INDEX-VAL         PIC 9(18) USAGE COMP-5.
       01  COUNTER-VAL       PIC 9 USAGE COMP-5.
       01  PRED-TAKEN        PIC 9 USAGE COMP-5.
       
       *> Byte Length for Bitwise Ops (8 bytes for PIC 9(18))
       01  BYTE-LEN          PIC 9(9) USAGE COMP-5 VALUE 8.
       
       *> Stats
       01  TOTAL-BRANCHES    PIC 9(18) USAGE COMP-5 VALUE 0.
       01  MISPREDICTIONS    PIC 9(18) USAGE COMP-5 VALUE 0.
       01  MISP-RATE         PIC 9(3)V9(5).
       *> Fix: Use 9.99 to force leading zero (e.g. 0.26)
       01  DISPLAY-RATE      PIC 9.99.
       
       *> Loop counters
       01  I                 PIC 9(9) USAGE COMP-5.
       01  STR-PTR           PIC 9(9) USAGE COMP-5.
       01  SPACE-COUNT       PIC 9(9) USAGE COMP-5.
       01  HEX-CHAR          PIC X.
       01  HEX-VAL           PIC 9(9) USAGE COMP-5.
       01  HEX-LEN           PIC 9(9) USAGE COMP-5.

       LINKAGE SECTION.
       *> We map the dynamic memory to this large array.
       01  PHT-MEM.
           05 PHT-ENTRY      PIC 9(1) USAGE COMP-5 OCCURS 1 TO 1073741824 
                             DEPENDING ON PHT-SIZE.

       PROCEDURE DIVISION.
       MAIN-LOGIC.
           
           PERFORM PARSE-ARGS
           PERFORM INIT-PREDICTOR
           PERFORM PROCESS-FILE
           PERFORM CALCULATE-STATS
           PERFORM CLEANUP
           STOP RUN.

       PARSE-ARGS.
           ACCEPT WS-CMD-ARGS FROM COMMAND-LINE
           
           UNSTRING WS-CMD-ARGS DELIMITED BY ALL SPACES
               INTO WS-TYPE, WS-M-STR, WS-N-STR, WS-FILENAME
           END-UNSTRING
           
           IF WS-TYPE NOT = "gshare"
               DISPLAY "Usage: sim gshare <GPB> <RB> <Trace_File>"
               STOP RUN 1
           END-IF
           
           MOVE FUNCTION NUMVAL(WS-M-STR) TO M
           MOVE FUNCTION NUMVAL(WS-N-STR) TO N
           
           IF M < 0 OR M > 30
               DISPLAY "Error: Invalid M (0-30)"
               STOP RUN 1
           END-IF
           
           IF N < 0 OR N > M
               DISPLAY "Error: Invalid N (0-M)"
               STOP RUN 1
           END-IF.

       INIT-PREDICTOR.
           COMPUTE PHT-SIZE = 2 ** M
           
           ALLOCATE PHT-SIZE CHARACTERS INITIALIZED RETURNING PHT-PTR
           
           IF PHT-PTR = NULL
               DISPLAY "Error: Memory Allocation Failed"
               STOP RUN 1
           END-IF
           
           SET ADDRESS OF PHT-MEM TO PHT-PTR
           
           PERFORM VARYING I FROM 1 BY 1 UNTIL I > PHT-SIZE
               MOVE 2 TO PHT-ENTRY(I)
           END-PERFORM
           
           MOVE 0 TO GHR.

       PROCESS-FILE.
           OPEN INPUT TRACE-FILE
           IF WS-FILE-STATUS NOT = "00"
               DISPLAY "Error opening file: " WS-FILENAME
               STOP RUN 1
           END-IF
           
           PERFORM UNTIL END-OF-FILE
               READ TRACE-FILE INTO WS-LINE
                   AT END
                       SET END-OF-FILE TO TRUE
                   NOT AT END
                       PERFORM PROCESS-LINE
               END-READ
           END-PERFORM
           
           CLOSE TRACE-FILE.

       PROCESS-LINE.
           *> 1. Sanitize Tabs
           INSPECT WS-LINE REPLACING ALL X"09" BY SPACE.
           
           *> 2. Sanitize Leading Spaces (Crucial fix for parsing)
           MOVE 0 TO SPACE-COUNT
           INSPECT WS-LINE TALLYING SPACE-COUNT FOR LEADING SPACES
           
           IF SPACE-COUNT >= LENGTH OF WS-LINE
               *> Line is empty or all spaces
               EXIT PARAGRAPH
           END-IF
           
           IF SPACE-COUNT > 0
               MOVE WS-LINE(SPACE-COUNT + 1:) TO WS-LINE
           END-IF.
           
           *> 3. Parse Cleaned Line
           MOVE SPACES TO WS-PC-HEX WS-OUTCOME-STR
           UNSTRING WS-LINE DELIMITED BY ALL SPACES
               INTO WS-PC-HEX WS-OUTCOME-STR
           END-UNSTRING
           
           IF WS-PC-HEX = SPACES OR WS-OUTCOME-STR = SPACES
               EXIT PARAGRAPH
           END-IF
           
           PERFORM HEX-TO-INT
           
           MOVE WS-OUTCOME-STR(1:1) TO WS-OUTCOME-CHAR
           IF WS-OUTCOME-CHAR = 't' OR WS-OUTCOME-CHAR = 'T'
               MOVE 1 TO ACTUAL-TAKEN
           ELSE
               MOVE 0 TO ACTUAL-TAKEN
           END-IF
           
           *> --- Index Calculation ---
           *> PC Index = (PC / 4)
           COMPUTE PC-INDEX = PC-VAL / 4
           COMPUTE MASK-VAL = (2 ** M) - 1
           
           *> Apply Mask: PC-INDEX = PC-INDEX AND MASK-VAL
           CALL "CBL_AND" USING MASK-VAL, PC-INDEX BY VALUE BYTE-LEN.
           
           *> GHR Shifted
           COMPUTE GHR-SHIFTED = GHR * (2 ** (M - N))
           
           *> XOR to get final Index: INDEX-VAL = PC-INDEX XOR GHR-SHIFTED
           MOVE PC-INDEX TO INDEX-VAL
           CALL "CBL_XOR" USING GHR-SHIFTED, INDEX-VAL BY VALUE BYTE-LEN.
           
           *> Adjust for 1-based array
           ADD 1 TO INDEX-VAL
           
           *> --- Prediction ---
           MOVE PHT-ENTRY(INDEX-VAL) TO COUNTER-VAL
           
           IF COUNTER-VAL >= 2
               MOVE 1 TO PRED-TAKEN
           ELSE
               MOVE 0 TO PRED-TAKEN
           END-IF
           
           IF PRED-TAKEN NOT = ACTUAL-TAKEN
               ADD 1 TO MISPREDICTIONS
           END-IF
           
           ADD 1 TO TOTAL-BRANCHES
           
           *> --- Update PHT ---
           IF ACTUAL-TAKEN = 1
               IF COUNTER-VAL < 3
                   ADD 1 TO PHT-ENTRY(INDEX-VAL)
               END-IF
           ELSE
               IF COUNTER-VAL > 0
                   SUBTRACT 1 FROM PHT-ENTRY(INDEX-VAL)
               END-IF
           END-IF
           
           *> --- Update GHR ---
           IF N > 0
               COMPUTE GHR = GHR / 2
               COMPUTE GHR = GHR + (ACTUAL-TAKEN * (2 ** (N - 1)))
           END-IF.

       HEX-TO-INT.
           MOVE 0 TO PC-VAL
           MOVE 0 TO HEX-LEN
           INSPECT FUNCTION REVERSE(WS-PC-HEX) TALLYING HEX-LEN FOR LEADING SPACES
           COMPUTE HEX-LEN = LENGTH OF WS-PC-HEX - HEX-LEN

           PERFORM VARYING STR-PTR FROM 1 BY 1 UNTIL STR-PTR > 20
               MOVE WS-PC-HEX(STR-PTR:1) TO HEX-CHAR
               IF HEX-CHAR = SPACE
                   EXIT PERFORM
               END-IF
               
               EVALUATE HEX-CHAR
                   WHEN '0' THRU '9'
                       COMPUTE HEX-VAL = FUNCTION NUMVAL(HEX-CHAR)
                   WHEN 'a' THRU 'f'
                       COMPUTE HEX-VAL = FUNCTION ORD(HEX-CHAR) - FUNCTION ORD('a') + 10
                   WHEN 'A' THRU 'F'
                       COMPUTE HEX-VAL = FUNCTION ORD(HEX-CHAR) - FUNCTION ORD('A') + 10
                   WHEN OTHER
                       EXIT PERFORM
               END-EVALUATE
               
               COMPUTE PC-VAL = (PC-VAL * 16) + HEX-VAL
           END-PERFORM.

       CALCULATE-STATS.
           IF TOTAL-BRANCHES > 0
               COMPUTE MISP-RATE = MISPREDICTIONS / TOTAL-BRANCHES
               MOVE MISP-RATE TO DISPLAY-RATE
           ELSE
               MOVE 0.00 TO DISPLAY-RATE
           END-IF
           
           MOVE M TO DISP-M
           MOVE N TO DISP-N
           
           *> Print nicely trimmed M, N and the Rate (0.26)
           DISPLAY FUNCTION TRIM(DISP-M) " " 
                   FUNCTION TRIM(DISP-N) " " 
                   DISPLAY-RATE.

       CLEANUP.
           FREE PHT-PTR.

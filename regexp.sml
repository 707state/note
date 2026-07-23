(* regexp.sml — Regular expression parsing and formatting *)

structure RegExp :> regexp =
   struct
      datatype regexp =
         Zero
       | One
       | Char of char
       | Plus of regexp * regexp
       | Times of regexp * regexp
       | Star of regexp

      exception SyntaxError of string

      (* Recursive descent parser.
       * Grammar:
       *   expr    -> seq ('+' seq)*
       *   seq     -> factor*
       *   factor  -> atom '*'?
       *   atom    -> char | '(' expr ')' | epsilon
       *)
      fun parse s =
         let
            val n = String.size s
            val pos = ref 0

            fun peek () = if !pos >= n then NONE else SOME (String.sub (s, !pos))
            fun next () = pos := !pos + 1

            fun parseExpr () =
               let
                  val r = parseSeq ()
               in
                  case peek () of
                     SOME #"+" => (next (); Plus (r, parseExpr ()))
                   | _ => r
               end

            and parseSeq () =
               case peek () of
                  NONE => One
                | SOME c =>
                     if c = #")" orelse c = #"+" then One
                     else
                        let
                           val f = parseFactor ()
                        in
                           case peek () of
                              NONE => f
                            | SOME c' =>
                                 if c' = #")" orelse c' = #"+" then f
                                 else Times (f, parseSeq ())
                        end

            and parseFactor () =
               let
                  val a = parseAtom ()
               in
                  case peek () of
                     SOME #"*" => (next (); Star a)
                   | _ => a
               end

            and parseAtom () =
               case peek () of
                  NONE => One
                | SOME #"(" =>
                     (next ()
                      ; let val r = parseExpr () in
                           case peek () of
                              SOME #")" => (next (); r)
                            | _ => raise SyntaxError "missing closing parenthesis"
                        end)
                | SOME c =>
                     if c = #")" orelse c = #"+" orelse c = #"*" then One
                     else (next (); Char c)
         in
            parseExpr ()
         end

      fun format r =
         case r of
            Zero => "0"
          | One => ""
          | Char c => String.str c
          | Plus (r1, r2) => "(" ^ format r1 ^ "+" ^ format r2 ^ ")"
          | Times (r1, r2) => format r1 ^ format r2
          | Star r =>
               let
                  val s = format r
               in
                  case r of
                     Char _ => s ^ "*"
                   | _ => "(" ^ s ^ ")*"
               end
   end

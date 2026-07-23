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

      open Lexer

      (* Token-based recursive descent parser.
       * Grammar (post-tokenization):
       *   expr    -> term ('+' term)*
       *   term    -> factor ('.' factor)*
       *   factor  -> atom '*'?
       *   atom    -> AtSign | Percent | Literal c | '(' expr ')' | epsilon
       *)
      fun parse s =
         let
            val toks = Lexer.tokenize s

            fun parseExpr (PlusSign :: _) = raise SyntaxError "unexpected '+'"
              | parseExpr toks =
                  case parseTerm toks of
                     (r, PlusSign :: rest) =>
                        let
                           val (r', remaining) = parseExpr rest
                              handle _ => raise SyntaxError "expected expression after '+'"
                        in
                           (Plus (r, r'), remaining)
                        end
                   | (r, rest) => (r, rest)

            and parseTerm (TimesSign :: _) = raise SyntaxError "unexpected '.'"
              | parseTerm toks =
                  case parseFactor toks of
                     (r, TimesSign :: rest) =>
                        let
                           val (r', rest') = parseTerm rest
                        in
                           (Times (r, r'), rest')
                        end
                   | (r, rest) => (r, rest)

            and parseFactor toks =
               case parseAtom toks of
                  (r, Asterisk :: rest) => (Star r, rest)
                | (r, rest) => (r, rest)

            and parseAtom (AtSign :: rest) = (Char #"@", rest)
              | parseAtom (Percent :: rest) = (Char #"%", rest)
              | parseAtom (Literal c :: rest) = (Char c, rest)
              | parseAtom (LParen :: rest) =
                  (case parseExpr rest of
                      (r, RParen :: rest') => (r, rest')
                    | (_, _) => raise SyntaxError "missing closing parenthesis")
              | parseAtom [] = (One, [])
              | parseAtom _ = raise SyntaxError "unexpected token"

            val (r, remaining) = parseExpr toks
         in
            if null remaining then r
            else raise SyntaxError "trailing tokens"
         end

      fun isSpecial c =
         c = #"+" orelse c = #"." orelse c = #"*"
         orelse c = #"(" orelse c = #")"
         orelse c = #"@" orelse c = #"%"
         orelse c = #"\\" orelse c = #" "

      fun format r =
         case r of
            Zero => "0"
          | One => ""
          | Char c =>
               if isSpecial c then "\\" ^ String.str c
               else String.str c
          | Plus (r1, r2) => format r1 ^ "+" ^ format r2
          | Times (r1, r2) =>
               let
                  val left =
                     case r1 of
                        Plus _ => "(" ^ format r1 ^ ")"
                      | _ => format r1
                  val right =
                     case r2 of
                        Plus _ => "(" ^ format r2 ^ ")"
                      | _ => format r2
               in
                  left ^ "." ^ right
               end
          | Star r =>
               let
                  val s = format r
               in
                  case r of
                     Char _ => s ^ "*"
                   | _ => "(" ^ s ^ ")*"
               end
   end

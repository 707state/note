(* lexer.sml — Tokenizer for regular expressions *)

structure Lexer : LEXER =
   struct
      datatype token =
         AtSign
       | Percent
       | Literal of char
       | PlusSign
       | TimesSign
       | Asterisk
       | LParen
       | RParen

      exception LexicalError

      fun tokenize s =
         let
            fun tok nil = nil
              | tok (#"+" :: cs) = PlusSign :: tok cs
              | tok (#"." :: cs) = TimesSign :: tok cs
              | tok (#"*" :: cs) = Asterisk :: tok cs
              | tok (#"(" :: cs) = LParen :: tok cs
              | tok (#")" :: cs) = RParen :: tok cs
              | tok (#"@" :: cs) = AtSign :: tok cs
              | tok (#"%" :: cs) = Percent :: tok cs
              | tok (#"\\" :: c :: cs) = Literal c :: tok cs
              | tok (#"\\" :: nil) = raise LexicalError
              | tok (#" " :: cs) = tok cs
              | tok (c :: cs) = Literal c :: tok cs
         in
            tok (String.explode s)
         end
   end

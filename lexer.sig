(* lexer.sig — Tokenizer signature for regular expressions *)

signature LEXER =
   sig
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

      val tokenize : string -> token list
   end

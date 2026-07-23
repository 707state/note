signature regexp = sig
  datatype regexp = 
  Zero | One | Char of char | 
  Plus of regexp * regexp | 
  Times of regexp * regexp | 
  Star of regexp
  exception SyntaxError of string
  val parse: string -> regexp
  val format: regexp -> string
end

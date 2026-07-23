signature matcher = sig
  structure RegExp: regexp
  val accepts : RegExp.regexp -> string -> bool
end

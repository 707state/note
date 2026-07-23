(* call-main.sml — program entry point with test demo *)

val _ = Main.main ()

val _ =
   let
      val r = RegExp.parse "(a+b)*"
      val m = Matcher.accepts r
      val ex1 = m "aabba"
      val ex2 = m "abac"
   in
      print "\n";
      print ("Demo: (a+b)*\n");
      print ("  matches \"aabba\" ->  " ^ Bool.toString ex1 ^ " (expected true)\n");
      print ("  matches \"abac\"  ->  " ^ Bool.toString ex2 ^ " (expected false)\n");
      print ("  matches \"\"      ->  " ^ Bool.toString (m "") ^ " (expected true)\n");
      print ("  matches \"a\"     ->  " ^ Bool.toString (m "a") ^ " (expected true)\n");
      print ("  matches \"b\"     ->  " ^ Bool.toString (m "b") ^ " (expected true)\n");
      print ("  matches \"c\"     ->  " ^ Bool.toString (m "c") ^ " (expected false)\n")
   end

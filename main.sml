(* main.sml — Test harness for regexp matcher *)

structure Main :> MAIN =
   struct
      fun test (name, actual, expected) =
         if actual = expected then
            print ("  PASS: " ^ name ^ "\n")
         else
            print ("  FAIL: " ^ name ^ " -- got " ^ Bool.toString actual
                   ^ ", expected " ^ Bool.toString expected ^ "\n")

      val testCases =
         [ ("(a+b)*", "aabba", true)
         , ("(a+b)*", "abac",  false)
         , ("(a+b)*", "",      true)
         , ("a*",     "",      true)
         , ("a*",     "aaa",   true)
         , ("a*",     "b",     false)
         , ("(a+b)*", "a",     true)
         , ("(a+b)*", "b",     true)
         , ("ab",     "ab",    true)
         , ("ab",     "ba",    false)
         , ("a+b",    "a",     true)
         , ("a+b",    "b",     true)
         , ("a+b",    "ab",    false)
         , ("(ab)*",  "abab",  true)
         , ("(ab)*",  "aba",   false)
         ]

      fun runTests () =
         let
            val r = RegExp.parse "(a+b)*"
            val m = Matcher.accepts r
            val _ = test ("Sanity: (a+b)* matches \"aabba\"", m "aabba", true)
            val _ = test ("Sanity: (a+b)* matches \"abac\"",  m "abac",  false)
            fun runOne (pat, s, expected) =
               let
                  val parsed = RegExp.parse pat
                  val m' = Matcher.accepts parsed
               in
                  test (pat ^ " matches \"" ^ s ^ "\"", m' s, expected)
               end
         in
            print ("--- Regexp Matcher Tests ---\n");
            app runOne testCases;
            print ("--- Done ---\n")
         end

      fun main () = runTests ()
   end

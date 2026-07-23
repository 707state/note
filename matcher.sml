(* matcher.sml — Brzozowski derivative-based regexp matcher *)

structure Matcher : matcher =
   struct
      structure RegExp = RegExp
      open RegExp

      (* nullable r  —  does r match the empty string? *)
      fun nullable Zero = false
        | nullable One = true
        | nullable (Char _) = false
        | nullable (Plus (r1, r2)) = nullable r1 orelse nullable r2
        | nullable (Times (r1, r2)) = nullable r1 andalso nullable r2
        | nullable (Star _) = true

      (* simplify r  —  algebraic simplification to contain growth *)
      fun simplify Zero = Zero
        | simplify One = One
        | simplify (Char c) = Char c
        | simplify (Plus (r1, r2)) =
            let
               val s1 = simplify r1
               val s2 = simplify r2
            in
               case (s1, s2) of
                  (Zero, _) => s2
                | (_, Zero) => s1
                | _ => Plus (s1, s2)
            end
        | simplify (Times (r1, r2)) =
            let
               val s1 = simplify r1
               val s2 = simplify r2
            in
               case (s1, s2) of
                  (Zero, _) => Zero
                | (_, Zero) => Zero
                | (One, _) => s2
                | (_, One) => s1
                | _ => Times (s1, s2)
            end
        | simplify (Star r) =
            let
               val s = simplify r
            in
               case s of
                  Zero => One
                | One => One
                | _ => Star s
            end

      (* derive r c  —  Brzozowski derivative of r w.r.t. character c *)
      fun derive r c =
         case r of
            Zero => Zero
          | One => Zero
          | Char c' => if c = c' then One else Zero
          | Plus (r1, r2) => Plus (derive r1 c, derive r2 c)
          | Times (r1, r2) =>
               if nullable r1 then
                  Plus (Times (derive r1 c, r2), derive r2 c)
               else
                  Times (derive r1 c, r2)
          | Star r => Times (derive r c, Star r)

      (* accepts r s  —  does the regexp r match the entire string s? *)
      fun accepts r s =
         let
            val n = String.size s
            fun go r i =
               if i >= n then
                  nullable r
               else
                  go (simplify (derive r (String.sub (s, i)))) (i + 1)
         in
            go (simplify r) 0
         end
   end

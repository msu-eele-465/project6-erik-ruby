/*
 * 5/05/25
 */
package coolStuff;

public class RollingAvg 
{
	private static float average = 0; 
	private static  int n=3, total =0, currentIdx = 0;
	private static int[] samples = new int[9];


	public static void main(String[] args)
	{
		int rand;
		for(int i = 1; i < 100; ++i)
		{
			rand = (int)(Math.random() * 100) + 1500;
			System.out.println("rand: " + rand);
			readValue(rand);
			if(i % 10 == 0)
			{
				changeN((int)(Math.random() * 9) + 1); // random n between 1 and 9
				System.out.println("N WAS CHANGED--------------------------");
			}
		}
	}

	private static void avgTemp()
	{
		average = (float)total / (float)n;
		System.out.println("Average (n = " + n + "): " + average);
	}

	private static void readValue(int reading)
	{
		samples[currentIdx] = reading;
		currentIdx++;
		if(currentIdx == n)
		{
			currentIdx = 0;

			total = 0;

			for(int i = 0; i < n; ++i)
			{
				System.out.print(samples[i] + ", ");
				total += samples[i];
			}
			System.out.println();
			avgTemp();
		}
	}

	private static void changeN(int newN)
	{
		n = newN;
		currentIdx = 0;
	}
}

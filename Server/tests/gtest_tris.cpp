// tests/test_gtest_tris.cpp
#include <gtest/gtest.h>
#include <vector>

extern "C"
{
    int validate_horizontal_alignment(unsigned int **topological_grid, int target_val);
    int validate_vertical_alignment(unsigned int **topological_grid, int target_val);
    int validate_diagonal_alignment(unsigned int **topological_grid, int target_val);
    int calculate_session_victor(unsigned int **topological_grid, unsigned int val1, unsigned int val2);
}

unsigned int **create_grid()
{
    unsigned int **grid = (unsigned int **)malloc(3 * sizeof(unsigned int *));
    for (int i = 0; i < 3; i++)
    {
        grid[i] = (unsigned int *)calloc(3, sizeof(unsigned int));
    }
    return grid;
}

void free_grid(unsigned int **grid)
{
    for (int i = 0; i < 3; i++)
        free(grid[i]);
    free(grid);
}

TEST(TrisLogic, HorizontalWin)
{
    unsigned int **grid = create_grid();
    grid[0][0] = 1;
    grid[0][1] = 1;
    grid[0][2] = 1;
    EXPECT_EQ(validate_horizontal_alignment(grid, 1), 1);
    EXPECT_EQ(calculate_session_victor(grid, 1, 2), 1);
    free_grid(grid);
}

TEST(TrisLogic, VerticalWin)
{
    unsigned int **grid = create_grid();
    grid[0][1] = 2;
    grid[1][1] = 2;
    grid[2][1] = 2;
    EXPECT_EQ(validate_vertical_alignment(grid, 2), 1);
    EXPECT_EQ(calculate_session_victor(grid, 1, 2), 2);
    free_grid(grid);
}

TEST(TrisLogic, DiagonalWin)
{
    unsigned int **grid = create_grid();
    grid[0][0] = 1;
    grid[1][1] = 1;
    grid[2][2] = 1;
    EXPECT_EQ(validate_diagonal_alignment(grid, 1), 1);
    EXPECT_EQ(calculate_session_victor(grid, 1, 2), 1);
    free_grid(grid);
}

TEST(TrisLogic, Draw)
{
    unsigned int **grid = create_grid();
    // X O X
    // X X O
    // O X O
    grid[0][0] = 1;
    grid[0][1] = 2;
    grid[0][2] = 1;
    grid[1][0] = 1;
    grid[1][1] = 1;
    grid[1][2] = 2;
    grid[2][0] = 2;
    grid[2][1] = 1;
    grid[2][2] = 2;
    EXPECT_EQ(calculate_session_victor(grid, 1, 2), -1);
    free_grid(grid);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}